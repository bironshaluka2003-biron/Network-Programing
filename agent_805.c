#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2805"
#define SID_TAG "SID:5082"

void *handle_client(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[1024] = {0};
    int authenticated = 0;
    int read_size;

    read_size = recv(sock, buffer, 1024, 0);
    if (read_size > 0) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        char *cmd = strtok(buffer, " ");
        char *token = strtok(NULL, " ");

        if (cmd != NULL && strcmp(cmd, "AUTH") == 0 && token != NULL && strcmp(token, AUTH_TOKEN) == 0) {
            authenticated = 1;
            char response[256];
            snprintf(response, sizeof(response), "OK AUTHENTICATED %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
            printf("Client authenticated successfully.\n");
        } else {
            char response[256];
            snprintf(response, sizeof(response), "ERR 001 AUTH FAILED %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
            printf("Client authentication failed.\n");
            close(sock);
            free(socket_desc);
            return NULL;
        }
    }

    while(authenticated && (read_size = recv(sock, buffer, 1024, 0)) > 0) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        
        if (strcmp(buffer, "QUIT") == 0) {
            char response[256];
            snprintf(response, sizeof(response), "OK BYE %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
            break;
        }
    }

    close(sock);
    free(socket_desc);
    return NULL;
}

int main() {
    int server_fd, *new_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Agent started. Listening on port %d...\n", PORT);

    while (1) {
        int client_sock;
        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        printf("Controller connected.\n");
        pthread_t sn_thread;
        new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        if (pthread_create(&sn_thread, NULL, handle_client, (void*)new_sock) < 0) {
            perror("could not create thread");
            free(new_sock);
            continue;
        }
        pthread_detach(sn_thread);
    }

    close(server_fd);
    return 0;
}
