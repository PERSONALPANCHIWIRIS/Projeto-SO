#include "subscription.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include "kvs.h"
#include "../common/io.h"


//-------------------------------------------------------------------------------------------CODE:

//Função de hash baseada na da hashtable dos jobs
static int hash(const char* key) {
    int firstLetter = tolower(key[0]);
    if (firstLetter >= 'a' && firstLetter <= 'z') {
        return firstLetter - 'a';
    } else if (firstLetter >= '0' && firstLetter <= '9') {
        return firstLetter - '0';
    }
    return -1;
}

SubscriptionMap* create_subscription_map() {
    SubscriptionMap* map = malloc(sizeof(SubscriptionMap));
    if (!map) return NULL;
    for (int i = 0; i < TABLE_SIZE; i++) {
        map->table[i] = NULL;
        pthread_mutex_init(&map->lock[i], NULL);
    }
    return map;
}

void free_subscription_map(SubscriptionMap* map) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Subscription* sub = map->table[i];
        while (sub != NULL) {
            Subscription* temp = sub;
            sub = sub->next;
            SubscriberNode* node = temp->subscribers;
            while (node != NULL) {
                SubscriberNode* temp_node = node;
                node = node->next;
                free(temp_node);
            }
            free(temp);
        }
        pthread_mutex_destroy(&map->lock[i]);
    }
    free(map);
}

int add_subscription(SubscriptionMap* map, const char* key, const char* notif_pipe_path) {
    int index = hash(key);

    // Verifica se existe no KVS
    char* value = read_pair(kvs_table, key);
    if (value == NULL) {
        //Não existe
        return 0;
    }
    free(value);

    pthread_mutex_lock(&map->lock[index]);

    Subscription* sub = map->table[index];
    while (sub != NULL) {
        if (strcmp(sub->key, key) == 0) {
            SubscriberNode* node = sub->subscribers;
            while (node != NULL) {
                if (strcmp(node->notif_pipe_path, notif_pipe_path) == 0) {
                    pthread_mutex_unlock(&map->lock[index]);
                    return 1; // Já subscreveu
                }
                //Encontrou um espaço vazio
                node = node->next;
            }
            //Para adicionar mais um subscritor a uma key
            node = malloc(sizeof(SubscriberNode));
            strcpy(node->notif_pipe_path, notif_pipe_path);
            node->next = sub->subscribers;
            sub->subscribers = node; //Adiciona o subscritor à frente
            pthread_mutex_unlock(&map->lock[index]);
            return 1; //Nao existia subscriçaõ
        }
        //Procura a key
        sub = sub->next;
    }
    sub = malloc(sizeof(Subscription));
    strcpy(sub->key, key);
    sub->subscribers = malloc(sizeof(SubscriberNode));
    strcpy(sub->subscribers->notif_pipe_path, notif_pipe_path);
    //Aponta para o NULL
    sub->subscribers->next = NULL;
    //Aponta para o proximo elemento
    sub->next = map->table[index];
    map->table[index] = sub;

    pthread_mutex_unlock(&map->lock[index]);
    return 1;
}

int remove_subscription(SubscriptionMap* map, const char* key, const char* notif_pipe_path) {
    int index = hash(key);

    // Verifica se existe no KVS
    if (read_pair(kvs_table, key) == NULL) {
        //Não existe
        return 1;
    }

    pthread_mutex_lock(&map->lock[index]);

    //Tira a subscription
    Subscription* sub = map->table[index];
    while (sub != NULL) {
        if (strcmp(sub->key, key) == 0) { //encontrou a key
            SubscriberNode* node = sub->subscribers; //O da frente na lista
            SubscriberNode* prev = NULL;
            while (node != NULL) {
                //Encontrar o subscriber, i.e, o pipe do cliente correspondente
                if (strcmp(node->notif_pipe_path, notif_pipe_path) == 0) {
                    if (prev) { //se o que quero tirar está no "meio" da lista de subscribers
                        prev->next = node->next;
                    } else {//O primeiro da lista
                        sub->subscribers = node->next;
                    }
                    free(node);
                    pthread_mutex_unlock(&map->lock[index]);
                    return 0; //Existia
                }
                //Continuamos iteradamente
                prev = node;
                node = node->next;
            }
        }
        //Procura a key
        sub = sub->next;
    }
    //Se não existir, só retorna 1
    pthread_mutex_unlock(&map->lock[index]);
    return 1;
}

void notify_subscribers(SubscriptionMap* map, const char* key, const char* message) {
    int index = hash(key);
    if (index == -1) return;

    pthread_mutex_lock(&map->lock[index]);

    Subscription* sub = map->table[index];
    while (sub  != NULL) {
        if (strcmp(sub->key, key) == 0) {
            SubscriberNode* node = sub->subscribers;
            while (node != NULL) {
                int fd = open(node->notif_pipe_path, O_WRONLY);
                if (fd != -1) {
                    char message_new_line[256];
                    //Escreve a mensagem no pipe
                    snprintf(message_new_line, sizeof(message_new_line), "%s\n", message);
                    write_all(fd, message_new_line, strlen(message_new_line));
                    close(fd);
                }
                node = node->next;
            }
            break;
        }
        //Procura a key
        sub = sub->next;
    }

    pthread_mutex_unlock(&map->lock[index]);
}

void remove_all_subscriptions(SubscriptionMap* map, const char* notif_pipe_path) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        pthread_mutex_lock(&map->lock[i]);
        Subscription* sub = map->table[i];
        while (sub != NULL) {
            SubscriberNode* node = sub->subscribers;
            SubscriberNode* prev = NULL;
            while (node != NULL) {
                if (strcmp(node->notif_pipe_path, notif_pipe_path) == 0) {
                    if (prev) {
                        prev->next = node->next;
                    } else {
                        sub->subscribers = node->next;
                    }
                    free(node);
                    break; // Remove apenas uma ocorrência por chave
                }
                prev = node;
                node = node->next;
            }
            sub = sub->next;
        }
        pthread_mutex_unlock(&map->lock[i]);
    }
}