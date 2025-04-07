#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define Buf_SIZE 1024

// Struct for thread args
struct client_info {
    int sock;
    int client_num;
    struct sockaddr_in addr;
};

// Global counters and mutexes
int global_msg_count = 0;
int client_id_counter = 0;
pthread_mutex_t msg_count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_id_lock = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg);
void error_handling(char *message);

int main(int argc, char *argv[]) {
    int ser_sock;
    struct sockaddr_in ser_adr, clnt_adr;
    socklen_t clnt_adr_sz;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    ser_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ser_sock == -1)
        error_handling("socket() error");

    memset(&ser_adr, 0, sizeof(ser_adr));
    ser_adr.sin_family = AF_INET;
    ser_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_adr.sin_port = htons(atoi(argv[1]));

    if (bind(ser_sock, (struct sockaddr *)&ser_adr, sizeof(ser_adr)) == -1)
        error_handling("bind() error");

    if (listen(ser_sock, 5) == -1)
        error_handling("listen() error");

    printf("Server listening on port %s...\n", argv[1]);

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        int client_sock = accept(ser_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }

        // Allocate and prepare client info struct
        struct client_info *cinfo = malloc(sizeof(struct client_info));
        cinfo->sock = client_sock;
        cinfo->addr = clnt_adr;

        // Safely increment and assign client number
        pthread_mutex_lock(&client_id_lock);
        client_id_counter++;
        cinfo->client_num = client_id_counter;
        pthread_mutex_unlock(&client_id_lock);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cinfo);
        pthread_detach(tid);
    }

    close(ser_sock);
    return 0;
}

void *handle_client(void *arg) {
    struct client_info *cinfo = (struct client_info *)arg;
    int sock = cinfo->sock;
    int client_num = cinfo->client_num;
    struct sockaddr_in client_addr = cinfo->addr;
    free(cinfo);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    char message[Buf_SIZE];
    printf("Client %d connected\n",client_num);
    while (1) {
        int str_len = read(sock, message, Buf_SIZE - 1);
        if (str_len <= 0)
            break;

        message[str_len] = '\0';

        // Update global message count safely
        pthread_mutex_lock(&msg_count_lock);
        global_msg_count++;
        int local_count = global_msg_count;
        pthread_mutex_unlock(&msg_count_lock);

        // Get current time
        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0';

        // Print to server console
        printf("[Client %d | %s:%d] ➜ %s\n", client_num, client_ip, client_port, message);

        // Prepare response
        char response[Buf_SIZE];
        snprintf(response, Buf_SIZE, "Echo: %.880s\tTimestamp: %.100s\tMessage Count: %d\n",
                 message, time_str, local_count);

        send(sock, response, strlen(response), 0);
        memset(message, 0, Buf_SIZE);
    }

    printf("Client %d (%s:%d) disconnected.\n", client_num, client_ip, client_port);
    close(sock);
    return NULL;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
