c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>

#define CONFIG_FILE "server.conf"
#define BUFFER_SIZE 48
#define MAX_CLIENTS 25
#define MAX_CHANNELS 25
#define CHANNEL_NAME_LEN 50
#define MAX_NICK_LENGTH 20

// Error codes
#define ERR_NEEDMOREPARAMS 101
#define ERR_NOSUCHCHANNEL 104
#define ERR_CHANNELISFULL 103
#define RPL_NOTOPIC 393

char NICK[BUFFER_SIZE];
int PORT;

typedef struct {
    char channelName[CHANNEL_NAME_LEN];
    int clients[MAX_CLIENTS];
    int clientCount;
    char topic[BUFFER_SIZE];
} Channel;

Channel channels[MAX_CHANNELS];
int channelCount = 0;

// Function declarations
void send_error(int client_socket, int error_code);
void handle_client(int client_socket);
void join_channel(int client_socket, const char *channelName);
void part_channel(int client_socket, const char *channelName);
void set_or_get_topic(int client_socket, const char *channelName, const char *topic);
void list_names(int client_socket, const char *channelName);
void read_config(const char *config_file);

// Error message sending
void send_error(int client_socket, int error_code) {
    const char *error_message;

    switch (error_code) {
        case ERR_NEEDMOREPARAMS:
            error_message = "Error 101: Need more parameters\n";
            break;
        case ERR_CHANNELISFULL:
            error_message = "Error 103: Channel is full\n";
            break;
        case ERR_NOSUCHCHANNEL:
            error_message = "Error 104: No such channel\n";
            break;
        default:
            error_message = "Unknown error\n";
            break;
    }

    send(client_socket, error_message, strlen(error_message), 0);
}

// Read server configuration
void read_config(const char *config_file) {
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL) {
        perror("Error opening configuration file");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (value) {
            value[strcspn(value, "\n")] = '\0'; // Remove newline character
            if (strcmp(key, "NICK") == 0) {
                strcpy(NICK, value);
            } else if (strcmp(key, "PORT") == 0) {
                PORT = atoi(value);
            }
        }
    }

    fclose(fp);
}

// Handle client connection
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            break; // Client disconnected
        }
        buffer[bytes_read] = '\0'; // Null-terminate the string

        // Process command (JOIN, PART, TOPIC)
        char *command = strtok(buffer, " \r\n");
        if (strcmp(command, "JOIN") == 0) {
            char *channel_name = strtok(NULL, " \r\n");
            join_channel(client_socket, channel_name);
        } else if (strcmp(command, "PART") == 0) {
            char *channel_name = strtok(NULL, " \r\n");
            part_channel(client_socket, channel_name);
        } else if (strcmp(command, "TOPIC") == 0) {
            char *channel_name = strtok(NULL, " \r\n");
            char *topic = strtok(NULL, "\r\n");
            set_or_get_topic(client_socket, channel_name, topic);
        }
    }

    close(client_socket);
    return NULL;
}

// Join a channel
void join_channel(int client_socket, const char *channelName) {
    if (channelCount >= MAX_CHANNELS) {
        send_error(client_socket, ERR_CHANNELISFULL);
        return;
    }

    for (int i = 0; i < channelCount; i++) {
        if (strcmp(channels[i].channelName, channelName) == 0) {
            // Channel exists, add client
            channels[i].clients[channels[i].clientCount++] = client_socket;
            return;
        }
    }

    // Create a new channel
    Channel newChannel;
    strncpy(newChannel.channelName, channelName, CHANNEL_NAME_LEN - 1);
    newChannel.clientCount = 1;
    newChannel.clients[0] = client_socket;
    channels[channelCount++] = newChannel;
}

// Part a channel
void part_channel(int client_socket, const char *channelName) {
    for (int i = 0; i < channelCount; i++) {
        if (strcmp(channels[i].channelName, channelName) == 0) {
            for (int j = 0; j < channels[i].clientCount; j++) {
                if (channels[i].clients[j] == client_socket) {
                    // Remove client from channel
                    for (int k = j; k < channels[i].clientCount - 1; k++) {
                        channels[i].clients[k] = channels[i].clients[k + 1];
                    }
                    channels[i].clientCount--;
                    return;
                }
            }
        }
    }
    send_error(client_socket, ERR_NOSUCHCHANNEL);
}

// Set or get the topic of a channel
void set_or_get_topic(int client_socket, const char *channelName, const char *topic) {
    for (int i = 0; i < channelCount; i++) {
        if (strcmp(channels[i].channelName, channelName) == 0) {
            if (topic) {
                strncpy(channels[i].topic, topic, sizeof(channels[i].topic) - 1);
                channels[i].topic[sizeof(channels[i].topic) - 1] = '\0';
                return;
            } else {
                // Send current topic
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "Current topic for %s: %s\n", channelName, channels[i].topic);
                send(client_socket, response, strlen(response), 0);
                return;
            }
        }
    }
    send_error(client_socket, ERR_NOSUCHCHANNEL);
}

int main() {
    read_config(CONFIG_FILE);

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t client_thread;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Accepting client failed");
            continue;
        }

        pthread_create(&client_thread, NULL, handle_client, (void *)&client_fd);
    }

    close(server_fd);
    return 0;
}
