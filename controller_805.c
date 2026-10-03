#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 9410
#define AUTH_TOKEN "OPS-2805"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    
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

    printf("Successfully connected to Agent on port %d\n", PORT);
    
    char auth_cmd[256];
    snprintf(auth_cmd, sizeof(auth_cmd), "AUTH %s\n", AUTH_TOKEN);
    send(sock, auth_cmd, strlen(auth_cmd), 0);
    
    int valread = read(sock, buffer, 1024);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("Agent Reply: %s", buffer);
    }

    send(sock, "QUIT\n", 5, 0);
    valread = read(sock, buffer, 1024);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("Agent Reply: %s", buffer);
    }
    
    close(sock);
    return 0;
}
