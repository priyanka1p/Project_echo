#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define BUF_SIZE 1024
#define LOG_FILE "log_server.txt"

void *handle_client(void *arg);
void error_handling(char *message);
void handle_sigint(int sig);

// Thread argument structure
struct client_info {
    int sock;
    int client_num;
    struct sockaddr_in addr;
};

pthread_mutex_t client_id_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
    int ser_sock;
    struct sockaddr_in ser_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    int client_id_counter = 0;
    pthread_t tid;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Ctrl+C handler
    signal(SIGINT, handle_sigint);

    // Socket creation
    ser_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ser_sock == -1)
        error_handling("socket() error");

    memset(&ser_adr, 0, sizeof(ser_adr));
    ser_adr.sin_family = AF_INET;
    ser_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_adr.sin_port = htons(atoi(argv[1]));

    // Binding
    if (bind(ser_sock, (struct sockaddr*)&ser_adr, sizeof(ser_adr)) == -1)
        error_handling("bind() error");

    // Listening
    if (listen(ser_sock, 5) == -1)
        error_handling("listen() error");

    printf("Server listening on port %s...\n", argv[1]);

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        int client_sock = accept(ser_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        if (client_sock == -1) {
            perror("accept() failed");
            continue;
        }

        // Allocate and pass client info to thread
        struct client_info *cinfo = malloc(sizeof(struct client_info));
        cinfo->sock = client_sock;
        cinfo->addr = clnt_adr;

        pthread_mutex_lock(&client_id_lock);
        client_id_counter++;
        cinfo->client_num = client_id_counter;
        pthread_mutex_unlock(&client_id_lock);

        pthread_create(&tid, NULL, handle_client, (void*)cinfo);
        pthread_detach(tid); // Auto-cleanup
    }

    close(ser_sock);
    return 0;
}

// Thread function to handle each client
void *handle_client(void *arg) {
    struct client_info *cinfo = (struct client_info *)arg;
    int sock = cinfo->sock;
    int client_num = cinfo->client_num;
    struct sockaddr_in client_addr = cinfo->addr;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);
    char message[BUF_SIZE];
    printf("Client connected: socket %d\n", sock);
    int message_count = 0;

    while (1) {
        int str_len = read(sock, message, BUF_SIZE - 1);
        if (str_len <= 0)
            break;

        message[str_len] = '\0';
        message_count++;

        time_t now = time(NULL); //returns the current time as the number of seconds
        char *time_str = ctime(&now);//converts into human readable form
        time_str[strlen(time_str) - 1] = '\0';//remove newline
        
        printf("[Client %d | %s:%d] ➜ %s\n", client_num, client_ip, client_port, message);

        char response[BUF_SIZE];
        snprintf(response, BUF_SIZE, "Echo: %.887s\tTimestamp: %.100s\tMessage Count: %d\n",
                 message, time_str, message_count);
        send(sock, response, strlen(response), 0);

        // Log to file
        pthread_mutex_lock(&log_lock);
        FILE *log_fp = fopen(LOG_FILE, "a");
        if (log_fp) {
            fprintf(log_fp, "[Client %d | %s:%d] ➜ %s | Count: %d | Time: %s\n",
                    client_num, client_ip, client_port, message, message_count, time_str);
            fclose(log_fp);
        }
        pthread_mutex_unlock(&log_lock);

        memset(message, 0, BUF_SIZE);
    }

    printf("Client disconnected: socket %d\n", sock);
    close(sock);
    free(cinfo);
    return NULL;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// Ctrl+C handler
void handle_sigint(int sig) {
    printf("\n//////////// Server Closed ////////////\n");

    // Log to file
    pthread_mutex_lock(&log_lock);
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp) {
        fprintf(log_fp, "\n//////////// Server Closed ////////////\n");
        fclose(log_fp);
    }
    pthread_mutex_unlock(&log_lock);

    exit(0);
}
