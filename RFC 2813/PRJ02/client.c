#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "base64_utils.h" // Include your Base64 utility header
#include <ctype.h>
#define CONFIG_FILE "client.conf"
#define BUFFER_SIZE 1024
#define QUIT_MESSAGE "QUIT"

char SERVER_IP[BUFFER_SIZE]; // Define SERVER_IP variable
int PORT; // Define PORT variable

void read_config(char *server_ip, int *port) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char *key, *value;

    fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        perror("Error opening configuration file");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        // Remove leading/trailing whitespace
        char *trimmed_line = strtok(line, " \t\r\n");
        if (trimmed_line == NULL || trimmed_line[0] == '#') {
            continue; // Skip empty lines and comments
        }

        key = strtok(trimmed_line, "=");
        value = strtok(NULL, "=");

        if (value == NULL) {
            fprintf(stderr, "Invalid configuration line: %s\n", line);
            continue;
        }

        // Remove trailing whitespace from value
        char *end = value + strlen(value) - 1;
        while (end >= value && isspace(*end)) {
            *end-- = '\0';
        }

        if (strcmp(key, "SERVER_IP") == 0) {
            strcpy(server_ip, value);
        } else if (strcmp(key, "PORT") == 0) {
            *port = atoi(value);
        }
    }

    fclose(fp);
}

// Function to continuously receive messages from the server
void *receive_messages(void *arg) {
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int valread;

    while (1) {
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            buffer[valread] = '\0'; // Null-terminate the received data
            printf("Server response: %s\n", buffer);
        }
    }

    return NULL;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char message[BUFFER_SIZE], encoded_password[BUFFER_SIZE];
    pthread_t recv_thread;
    
    // Read configuration from client.conf
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

    // Create a thread to receive messages from the server
    pthread_create(&recv_thread, NULL, receive_messages, (void *)&sock);

    while (1) {
        //printf("Enter command:\n");
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0; // Remove newline character for clean processing

        // Check if the command starts with "NICK"
        if (strncmp(message, "NICK", 4) == 0) {
            char *nickname = strtok(message + 5, " "); // Skip "NICK " and get nickname
            char *password = strtok(NULL, ""); // Get the rest (password)

            if (password && password[0] == ':') {
                password++; // Skip ':'
                base64_encode((const unsigned char *)password, strlen(password), encoded_password);
                //printf("Encoded password: %s\n", encoded_password); // Debug print of the encoded password
                snprintf(message, BUFFER_SIZE, "NICK %s :%s", nickname, encoded_password);
            }
        } else if (strncmp(message, "PASS", 4) == 0) {
            char *password = message + 5; // Pointer arithmetic to skip "PASS "
            if (*password == ':') password++; // Skip ':' if directly after "PASS"
            base64_encode((unsigned char *)password, strlen(password), encoded_password);
            snprintf(message, BUFFER_SIZE, "PASS %s", encoded_password); // Prepare the full command with encoded password
            //printf("Encoded password: %s\n", encoded_password); // Debug print of the encoded password
        }

        // Send message to server
        send(sock, message, strlen(message), 0);

        // Check if message is "QUIT"
        if (strcmp(message, "QUIT") == 0) {
            break; // Exit loop and close connection
        }
    }

    close(sock); // Close connection

    return 0;
}