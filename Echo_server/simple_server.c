#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>

#define Buf_SIZE 1024

void error_handling(char *message);

int main(int argc, char *argv[]) {
    int ser_sock, client_sock;
    char message[Buf_SIZE];
    int str_len, message_count = 0;

    struct sockaddr_in ser_adr;
    struct sockaddr_in clnt_adr;
    socklen_t clnt_adr_sz;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
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
    if (listen(ser_sock, 1) == -1)
        error_handling("listen() error");

    clnt_adr_sz = sizeof(clnt_adr);
    client_sock = accept(ser_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
    if (client_sock == -1)
        error_handling("accept() error");
    else
        printf("Connected to client\n");

    while ((str_len = read(client_sock, message, Buf_SIZE)) != 0) {
        message[str_len-1] = '\0'; // Ensure null termination
        message_count++;
        // Get current server time
        time_t now = time(NULL);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0'; // Remove newline

        // Prepare response
        char response[Buf_SIZE];
	snprintf(response, Buf_SIZE,"Echo: %.880s\tTimestamp: %.100s\tMessage Count: %d\n",message, time_str, message_count);
	// Send response
        send(client_sock, response, strlen(response), 0);
        memset(message, 0, Buf_SIZE);
    }

    close(client_sock);
    close(ser_sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
