#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2805"

void send_command(int sock, const char *cmd) {
    char buffer[2048] = {0};
    send(sock, cmd, strlen(cmd), 0);
    int valread = read(sock, buffer, 2048);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("Agent: %s", buffer);
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address / Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed!\n");
        return -1;
    }

    char auth_cmd[256];
    snprintf(auth_cmd, sizeof(auth_cmd), "AUTH %s\n", AUTH_TOKEN);
    send_command(sock, auth_cmd);
    
    send_command(sock, "SYSINFO\n");
    send_command(sock, "LISTPROC\n");
    send_command(sock, "EXEC WHOAMI\n");
    send_command(sock, "PUT config.txt\n"); 
    send_command(sock, "GET log.txt\n"); 

    send_command(sock, "MONITOR START 8080\n");
    sleep(5);
    send_command(sock, "MONITOR STOP\n");



    send_command(sock, "QUIT\n");
    
    close(sock);
    return 0;
}
