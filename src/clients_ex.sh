#!/bin/bash

# Remover FIFO antigo, se necessário
rm -f register_fifo

# Iniciar servidor em segundo plano
./server/kvs ./server/jobs 3 3 register_fifo &

# Salvar o PID do servidor para controle posterior (opcional)
SERVER_PID=$!

# Iniciar os clientes em segundo plano
./client/client c2 register_fifo < ./client/client_tests.txt &
./client/client c3 register_fifo < ./client/client_tests2.txt &

# Aguardar a conclusão dos clientes
wait

# (Opcional) Terminar o servidor após os clientes concluírem
kill $SERVER_PID
