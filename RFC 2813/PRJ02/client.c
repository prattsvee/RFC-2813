c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "base64_utils.h"
#include <ctype.h>

#define CONFIG_FILE "client.conf"
#define BUFFER_SIZE 1024
#define QUIT_MESSAGE "QUIT"

char SERVER_IP[BUFFER_SIZE];
int PORT;

void read_config(char *server_ip, int *port) {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        perror("Error opening configuration file");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed_line = strtok(line, " \t\r\n");
        if (trimmed_line == NULL || trimmed_line[0] == '#') {
            continue; // Skip empty lines and comments
        }

        char *key = strtok(trimmed_line, "=");
        char *value = strtok(NULL, "=");
        if (value == NULL) {
            fprintf(stderr, "Invalid configuration line: %s\n", line);
            continue;
        }

        char *end = value + strlen(value) - 1;
        while (end >= value && isspace(*end)) {
            *end-- = '\0'; // Remove trailing whitespace
        }

        if (strcmp(key, "SERVER_IP") == 0) {
            strcpy(server_ip, value);
        } else if (strcmp(key, "PORT") == 0) {
            *port = atoi(value);
        }
    }

    fclose(fp);
}

void *receive_messages(void *arg) {
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t valread = read(sock, buffer, sizeof(buffer) - 1);
        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate the received data
            printf("Server response: %s\n", buffer);
        } else if (valread == 0) {
            printf("Server closed connection.\n");
            break;
        } else {
            perror("Error reading from socket");
            break;
        }
    }

    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char message[BUFFER_SIZE], encoded_password[BUFFER_SIZE];
    pthread_t recv_thread;

    read_config(SERVER_IP, &PORT);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    pthread_create(&recv_thread, NULL, receive_messages, (void *)&sock);

    while (1) {
        printf("Enter command:\n");
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0; // Remove newline character

        // Command handling (NICK and PASS)
        if (strncmp(message, "NICK", 4) == 0 || strncmp(message, "PASS", 4) == 0) {
            char *cmd = strtok(message, " ");
            char *param = strtok(NULL, " ");
            if (param) {
                if (strncmp(cmd, "NICK", 4) == 0) {
                    char *password = strtok(NULL, ":");
                    if (password) {
                        password++; // Skip ':'
                        base64_encode((const unsigned char *)password, strlen(password), encoded_password);
                        snprintf(message, BUFFER_SIZE, "NICK %s :%s", param, encoded_password);
                    }
                } else if (strncmp(cmd, "PASS", 4) == 0) {
                    base64_encode((unsigned char *)param, strlen(param), encoded_password);
                    snprintf(message, BUFFER_SIZE, "PASS %s", encoded_password);
                }
            }
        }

        // Send message to server
        send(sock, message, strlen(message), 0);

        // Check for QUIT command
        if (strcmp(message, QUIT_MESSAGE) == 0) {
            break; // Exit loop and close connection
        }
    }

    close(sock); // Close connection
    return 0;
}
