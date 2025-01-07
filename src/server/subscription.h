#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <stddef.h>
#include <pthread.h>
#include "constants.h"

#define MAX_SUBSCRIBERS 10
#define MAX_PIPE_PATH 256
#define TABLE_SIZE 26

typedef struct SubscriberNode {
    char notif_pipe_path[MAX_PIPE_PATH];
    struct SubscriberNode* next;
} SubscriberNode;

typedef struct Subscription {
    char key[MAX_STRING_SIZE];
    SubscriberNode* subscribers; //Lista de nodes (subscribers)
    struct Subscription* next;
} Subscription;

typedef struct SubscriptionMap {
    Subscription* table[TABLE_SIZE];
    pthread_mutex_t lock[TABLE_SIZE];
} SubscriptionMap;

SubscriptionMap* create_subscription_map();
void free_subscription_map(SubscriptionMap* map);
int add_subscription(SubscriptionMap* map, const char* key, const char* notif_pipe_path);
int remove_subscription(SubscriptionMap* map, const char* key, const char* notif_pipe_path);
void remove_all_subscriptions(SubscriptionMap* map, const char* notif_pipe_path); 
void notify_subscribers(SubscriptionMap* map, const char* key, const char* message);

#endif // SUBSCRIPTION_H