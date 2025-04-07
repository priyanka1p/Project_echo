#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>

#define Buf_SIZE 1024

void *handle_client(void *arg);
void error_handling(char *message);

// Thread argument structure
struct client_info {
    int sock;
    struct sockaddr_in addr;
};

int main(int argc, char *argv[]) {
    int ser_sock;
    struct sockaddr_in ser_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t tid;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

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

        // Create a thread for each client
        pthread_create(&tid, NULL, handle_client, (void*)cinfo);//it is non blocking, it creates thread i.e stack and reg are allocated with common heap,DS, CS 
        pthread_detach(tid); // Automatically free thread resources once completed 
        //pthread_join(tid,NULL); it will wait for one thread to complete its execution and gives access to other threads.
    }

    close(ser_sock);
    return 0;
}

// Thread function to handle each client
void *handle_client(void *arg) {
    struct client_info *cinfo = (struct client_info *)arg;
    int sock = cinfo->sock;
    char message[Buf_SIZE];
    int str_len, message_count = 0;

    printf("Client connected: socket %d\n", sock);

    while ((str_len = read(sock, message, Buf_SIZE)) > 0) {
        message[str_len - 1] = '\0'; // Null-terminate
        message_count++;

        time_t now = time(NULL); //returns the current time as the number of seconds
        char *time_str = ctime(&now);//converts into human readable form
        time_str[strlen(time_str) - 1] = '\0'; // Remove newline

        char response[Buf_SIZE];
        snprintf(response, Buf_SIZE, "Echo: %.887s\tTimestamp: %.100s\tMessage Count: %d\n", message, time_str, message_count);

        send(sock, response, strlen(response), 0);
        memset(message, 0, Buf_SIZE);
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
