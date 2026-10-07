#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

typedef struct {
    char client_ip[64];
    int udp_port;
    int *monitor_active;
} MonitorArgs;

void* udp_monitor_thread(void* arg) {
    MonitorArgs* args = (MonitorArgs*)arg;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serveraddr;

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(args->udp_port);
    inet_pton(AF_INET, args->client_ip, &serveraddr.sin_addr);

    while (*(args->monitor_active)) {
        char message[256];
        snprintf(message, sizeof(message), "SYSINFO 15%% 1024MB 3600s SID:5082\n");
        sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));
        sleep(3);
    }

    close(sockfd);
    free(args);
    return NULL;
}

#define PORT 9410
#define AUTH_TOKEN "OPS-2805"
#define SID_TAG "SID:5082"

void log_action(const char *action) {
    FILE *log_file = fopen("remoteops_IT24102805.log", "a");
    if (log_file != NULL) {
        fprintf(log_file, "%s\n", action);
        fclose(log_file);
    }
}

void *handle_client(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[1024] = {0};
    int authenticated = 0;

    log_action("Client Connected");

    int monitor_active = 0;
    pthread_t monitor_tid;
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    char client_ip[64];
    getpeername(sock, (struct sockaddr*)&peer_addr, &peer_len);
    inet_ntop(AF_INET, &(peer_addr.sin_addr), client_ip, sizeof(client_ip));

    int read_size;

    read_size = recv(sock, buffer, 1024, 0);
    if (read_size > 0) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        char *cmd = strtok(buffer, " ");
        char *token = strtok(NULL, " ");

        if (cmd != NULL && strcmp(cmd, "AUTH") == 0 && token != NULL && strcmp(token, AUTH_TOKEN) == 0) {
            authenticated = 1;
            log_action("Client Authenticated");

            char response[256];
            snprintf(response, sizeof(response), "OK AUTHENTICATED %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        } else {
            char response[256];
            snprintf(response, sizeof(response), "ERR 001 AUTH FAILED %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
            close(sock);
            free(socket_desc);
            return NULL;
        }
    }

    while(authenticated && (read_size = recv(sock, buffer, 1024, 0)) > 0) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        char response[2048] = {0};
        
        if (strcmp(buffer, "QUIT") == 0) {
            log_action("Command Received: BYE");
            snprintf(response, sizeof(response), "OK BYE %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
            break;
        } 
        else if (strcmp(buffer, "SYSINFO") == 0) {
            log_action("Command Received: SYSINFO");
            snprintf(response, sizeof(response), "OK SYSINFO 15%% 1024MB 3600s %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        }
        else if (strcmp(buffer, "LISTPROC") == 0) {
            log_action("Command Received: PROCS");
            snprintf(response, sizeof(response), "OK PROCS init,sshd,bash %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "EXEC ", 5) == 0) {
            log_action("Command Received: EXEC");
            char *exec_cmd = buffer + 5;
            if (strcmp(exec_cmd, "DATE") == 0 || strcmp(exec_cmd, "UPTIME") == 0 || 
                strcmp(exec_cmd, "DISKFREE") == 0 || strcmp(exec_cmd, "HOSTNAME") == 0 || 
                strcmp(exec_cmd, "WHOAMI") == 0) {
                snprintf(response, sizeof(response), "OK EXEC_RESULT Command_%s_Executed_Successfully %s\n", exec_cmd, SID_TAG);
            } else {
                snprintf(response, sizeof(response), "ERR 002 COMMAND NOT ALLOWED %s\n", SID_TAG);
            }
            send(sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "PUT ", 4) == 0) {
            log_action("Command Received: PUT");
            snprintf(response, sizeof(response), "OK PUT_SUCCESS File_Received %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "GET ", 4) == 0) {
            log_action("Command Received: GET");
            snprintf(response, sizeof(response), "OK GET_SUCCESS File_Data_Sent %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "MONITOR START", 13) == 0) {
            log_action("Command Received: MONITOR START");
            int udp_port;
            sscanf(buffer, "MONITOR START %d", &udp_port);
            
            if (!monitor_active) {
                monitor_active = 1;
                MonitorArgs* m_args = malloc(sizeof(MonitorArgs));
                strcpy(m_args->client_ip, client_ip);
                m_args->udp_port = udp_port;
                m_args->monitor_active = &monitor_active;
                
                pthread_create(&monitor_tid, NULL, udp_monitor_thread, m_args);
                pthread_detach(monitor_tid);
            }
            snprintf(response, sizeof(response), "OK MONITOR STARTED %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        }
        else if (strncmp(buffer, "MONITOR STOP", 12) == 0) {
            log_action("Command Received: MONITOR STOP");
            monitor_active = 0;
            snprintf(response, sizeof(response), "OK MONITOR STOPPED %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
        }
        else {
            snprintf(response, sizeof(response), "ERR 003 UNKNOWN COMMAND %s\n", SID_TAG);
            send(sock, response, strlen(response), 0);
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
