#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>  

#define BUF_SIZE 1024
#define MAX_CLIENTS 100
#define LOG_FILE "log_server.txt"

pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

int global_msg_count = 0;
int client_counter = 0;

// Struct for thread args
struct client_info {
    int sock;
    int client_num;
    struct sockaddr_in addr;
};

void error_handling(const char *message) {
    perror(message);
    exit(1);
}

// Signal handler
void handle_sigint(int sig) {
    printf("\n/////////////////// Server shutting down ///////////////////\n");

    pthread_mutex_lock(&log_lock);
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp != NULL) {
        fprintf(log_fp, "\n/////////////////// Server shutting down ///////////////////\n\n");
        fclose(log_fp);
    } else {
        perror("Error writing shutdown message to log");
    }
    pthread_mutex_unlock(&log_lock);

    exit(0);
}

void log_message(int client_num, const char *message, const char *timestamp, int msg_count) {
    pthread_mutex_lock(&log_lock);

    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp == NULL) {
        perror("Error opening log file");
        pthread_mutex_unlock(&log_lock);
        return;
    }

    fprintf(log_fp, "Client %d: %s\nTimestamp: %s\nMessage Count: %d\n\n",
            client_num, message, timestamp, msg_count);

    fclose(log_fp);
    pthread_mutex_unlock(&log_lock);
}

void *handle_client(void *arg) {
    char buffer[BUF_SIZE];
    int str_len;
    struct client_info *cinfo = (struct client_info *)arg;
    int sock = cinfo->sock;
    int client_num = cinfo->client_num;
    struct sockaddr_in client_addr = cinfo->addr;
    free(cinfo);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    printf("Client %d connected from %s:%d\n", client_num, client_ip, client_port);

    while ((str_len = read(sock, buffer, BUF_SIZE - 1)) > 0) {
        buffer[str_len] = '\0';

        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0';

        pthread_mutex_lock(&count_lock);
        global_msg_count++;
        int msg_count = global_msg_count;
        pthread_mutex_unlock(&count_lock);

        printf("[Client %d | %s:%d] ➜ %s\n", client_num, client_ip, client_port, buffer);

        char response[BUF_SIZE];
        snprintf(response, BUF_SIZE,
                 "Echo: %.800s\nTimestamp: %.100s\nMsg Count: %d\n",
                 buffer, timestamp, msg_count);

        send(sock, response, strlen(response), 0);

        log_message(client_num, buffer, timestamp, msg_count);
    }

    printf("Client %d disconnected.\n", client_num);
    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    signal(SIGINT, handle_sigint); 

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
        error_handling("socket");

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        error_handling("bind");

    if (listen(server_sock, MAX_CLIENTS) == -1)
        error_handling("listen");

    printf("Server listening on port %s...\n", argv[1]);

    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&count_lock);
        client_counter++;
        int client_num = client_counter;
        pthread_mutex_unlock(&count_lock);

        struct client_info *cinfo = malloc(sizeof(struct client_info));
        cinfo->sock = client_sock;
        cinfo->client_num = client_num;
        cinfo->addr = client_addr;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)cinfo);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
