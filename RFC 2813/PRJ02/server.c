#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <asm-generic/socket.h>
#define CONFIG_FILE "server.conf"

#define BUFFER_SIZE 48
#define MAX_CLIENTS 25
#define MAX_CHANNELS 25
#define CHANNEL_NAME_LEN 50
#define MAX_NICK_LENGTH 20
#define MAX_SOCKET_ADDRESSES 5

#define ERR_NEEDMOREPARAMS 101
#define ERR_TOOMANYPARAMS 102
#define ERR_CHANNELISFULL 103
#define ERR_NOSUCHCHANNEL 104
#define ERR_TOOMANYCHANNELS 105
#define ERR_TOOMANYTARGETS 106
#define ERR_UNAVAILRESOURCE 107
#define ERR_NORECIPIENT 304
#define ERR_NOTEXTTOSEND 306
#define ERR_NOSUCHNICK 309
#define ERR_INCORRECT_PASS 501

#define ERR_NOTONCHANNEL 201
#define ERR_NOSUCHNICK_OR_NOSUCHCHANNEL 401
#define ERR_TOOMANYMATCHES 301
#define ERR_NOSUCHSERVER 302
#define ERR_NOSUCHSERVER 402
#define RPL_TIME 391
#define RPL_NOTOPIC 393

#define MAX_CONNECTED_SERVERS 10

char NICK[BUFFER_SIZE]; // Declare NICK as a global variable
int PORT;               // Declare PORT as a global variable

void send_error(int client_socket, int error_code)
{
    const char *error_message;
    switch (error_code)
    {
    case ERR_NEEDMOREPARAMS:
        error_message = "Error 101: Need more parameters";
        break;
    case ERR_TOOMANYPARAMS:
        error_message = "Error 102: Too many parameters";
        break;
    case ERR_CHANNELISFULL:
        error_message = "Error 103: Channel is full";
        break;
    case ERR_NOSUCHCHANNEL:
        error_message = "Error 104: No such channel";
        break;
    case ERR_TOOMANYCHANNELS:
        error_message = "Error 105: Too many channels";
        break;
    case ERR_TOOMANYTARGETS:
        error_message = "Error 106: Too many targets";
        break;
    case ERR_UNAVAILRESOURCE:
        error_message = "Error 107: Unavailable resource";
        break;
    case ERR_NOTONCHANNEL:
        error_message = "Error 201: Not on channel";
        break;
    case ERR_TOOMANYMATCHES:
        error_message = "Error 301: Too many matches";
        break;
    case ERR_NOSUCHSERVER:
        error_message = "Error 302: No such server";
        break;
    case ERR_NORECIPIENT:
        error_message = ":%s 411 :No recipient given (PRIVMSG)\n";
        break;
    case ERR_NOTEXTTOSEND:
        error_message = ":%s 412 :No text to send\n";
        break;
    case ERR_NOSUCHNICK_OR_NOSUCHCHANNEL:
        error_message = ":%s 401 :No such nick/channel\n";
        break;
    case RPL_NOTOPIC:
        error_message = "No topic";
    case ERR_INCORRECT_PASS:
        error_message = "Incorrect Password try again";
    default:
        error_message = "Unknown error";
    }

    send(client_socket, error_message, strlen(error_message), 0);
    //printf("Sent to client %d: %s\n", client_socket, error_message);
}

char nicknames[MAX_CLIENTS][MAX_NICK_LENGTH]; // Array to store nicknames

typedef struct
{
    char nickname[MAX_NICK_LENGTH];
    char realname[BUFFER_SIZE];
    int client_socket;
    char password[BUFFER_SIZE]; // Using 'password' for another purpose could be confusing.
    int hopcount;
    char username[BUFFER_SIZE];
    char host[BUFFER_SIZE];
    int servertoken;
    char umode[BUFFER_SIZE];
    struct sockaddr_in address; // Storing the IP address and port
} UserInfo;

UserInfo user_info[MAX_CLIENTS]; // Array to store user information

typedef struct
{

    char channelName[CHANNEL_NAME_LEN];
    int clients[MAX_CLIENTS];
    int clientCount;
    char topic[BUFFER_SIZE];
} Channel;

// typedef struct {
//     char ip_address[16];
//     int server_port;
// } ConnectedServerInfo;

typedef struct
{
    char nick[MAX_NICK_LENGTH];
    char pass[BUFFER_SIZE];
    int client_port;
    int serverCount;
    struct
    {
        char ip[INET_ADDRSTRLEN];
        int server_port;
    } sockAddr[MAX_SOCKET_ADDRESSES];
} ServerInfo;

typedef struct
{
    char ip[INET_ADDRSTRLEN];
    int server_port;
} ServerAddress;

Channel channels[MAX_CHANNELS];
int channelCount = 0;
ServerInfo serverConfig[MAX_CONNECTED_SERVERS];
ServerInfo server_info;
UserInfo user_info[MAX_CLIENTS];

void *handle_server(void *arg);
void *receive_messages(void *arg);
// void print_server_info(ConnectedServerInfo connected_servers[], int noOfServers);
void *handle_client(void *arg);
void handle_nick_command(int client_socket, char *params[], int num_params);
void handle_user_command(int client_socket, const char *nickname, const char *realname);
void join_channel(int client_socket, const char *channelName);
void part_channel(int client_socket, const char *channelName);
void set_or_get_topic(int client_socket, const char *channelName, const char *topic);
void list_names(int client_socket, const char *channelName);
void handle_privmsg(int client_socket, const char *msgtarget, const char *message);
void remove_extra_spaces(char *str);
void handle_time_command(int client_socket);
void handle_pass_command(int client_socket, const char *password);
void save_password_to_file(int client_socket, const char *password);
// void read_config(const char *config_file, char *server_nick, int *port, int *noOfServers, ConnectedServerInfo connected_servers[]);
void read_config(const char *config_file, ServerInfo *server_info);

// S2S
void handle_server_nick(int client_socket, char *params[], int num_params);
void handle_server_pass(int client_socket, char *params[], int num_params);
void handle_server_squit(int client_socket, char *params[], int num_params);
void handle_server_njoin(int client_socket, char *params[], int num_params);
void handle_server_privmsg(char *params[], int num_params);

void read_config(const char *config_file, ServerInfo *server_info)
{
    FILE *fp;
    char line[100];
    int server_count = 0;

    fp = fopen(config_file, "r");
    if (fp == NULL)
    {
        perror("Error opening configuration file");
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (value == NULL)
        {
            continue;
        }

        value[strcspn(value, "\n")] = '\0'; // Remove trailing newline character

        if (strcmp(key, "NICK") == 0)
        {
            strcpy(server_info->nick, value);
        }
        else if (strcmp(key, "PASS") == 0)
        {
            strcpy(server_info->pass, value);
        }
        else if (strcmp(key, "PORT") == 0)
        {
            server_info->client_port = atoi(value);
        }
        else if (strcmp(key, "SERVERS") == 0)
        {
            server_info->serverCount = atoi(value);
        }
        else if (strcmp(key, "SOCK_ADDR") == 0 && server_count < MAX_SOCKET_ADDRESSES)
        {
            char *ip_port = strtok(value, ":");
            char *port_str = strtok(NULL, ":");

            if (ip_port != NULL && port_str != NULL)
            {
                strcpy(server_info->sockAddr[server_count].ip, ip_port);
                server_info->sockAddr[server_count].server_port = atoi(port_str);
                server_count++;
            }
        }
    }

    fclose(fp);
}

// void print_server_info(ConnectedServerInfo connected_servers[], int noOfServers) {
//     printf("Connected Servers:\n");
//     for (int i = 0; i < noOfServers; i++) {
//         printf("Server %d: IP Address: %s, Port: %d\n", i + 1, connected_servers[i].ip_address, connected_servers[i].server_port);
//     }
// }

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t client_thread, server_threads[MAX_CONNECTED_SERVERS];
    int opt = 1;
    // ConnectedServerInfo connected_servers[noOfServers];

    read_config(argv[1], &server_info);

    // Create server socket for clients
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_info.client_port);

    // Bind server socket to the client port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections from clients
    if (listen(server_fd, MAX_CLIENTS) == -1)
    {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < server_info.serverCount; i++)
    {
        ServerAddress *server_address = malloc(sizeof(ServerAddress));
        if (server_address == NULL)
        {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        snprintf(server_address->ip, INET_ADDRSTRLEN, "%s", server_info.sockAddr[i].ip);
        server_address->server_port = server_info.sockAddr[i].server_port;

        if (pthread_create(&server_threads[i], NULL, handle_server, (void *)server_address) != 0)
        {
            perror("Server thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    while (1)
    {

        // Accept incoming client connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1)
        {
            perror("Accepting client failed");
            exit(EXIT_FAILURE);
        }

        // Populate user_info with client information
        int index;
        for (index = 0; index < MAX_CLIENTS; index++)
        {
            if (user_info[index].client_socket == 0)
            { // Find an empty slot in user_info array
                user_info[index].client_socket = client_fd;
                printf("Client connected with socket %d\n", user_info[index].client_socket); // Debug output
                break;
            }
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)&client_fd) != 0)
        {
            perror("pthread_create");
            close(client_fd);
        }
    }

    return 0;
}

void *handle_server(void *arg)
{
    char message[BUFFER_SIZE];
    ServerAddress *server_address = (ServerAddress *)arg;
    int server_port = *((int *)arg);
    int server_fd;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    // Create server socket for server-to-server communication
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Server socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure for server-to-server communication
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_address->server_port);

    if (inet_pton(AF_INET, server_address->ip, &server_addr.sin_addr) <= 0)
    { // here SERVER_IP is ip address of another server which will act as server for this server(here act as client)
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // Attempt to connect to the remote server
    while (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        // perror("Server connection failed, retrying...");
        sleep(1); // Wait for 1 second before retrying
    }

    // Debug message indicating successful connection
    printf("Server-to-server connection established on port %d\n", server_address->server_port);

    // Handle server-to-server communication here i.e we will write server-server commands here(yet to confirm)

    // Create a thread to receive messages from the server
    pthread_create(&recv_thread, NULL, receive_messages, (void *)&server_fd);

    while (1)
    {
        fgets(message, BUFFER_SIZE, stdin);

        // Send message to server
        send(server_fd, message, strlen(message), 0);

        // Check if message is "QUIT"
        if (strncmp(message, "SQUIT", strlen("SQUIT")) == 0)
        {
            break; // Exit loop and close connection
        }
    }

    close(server_fd);
    pthread_exit(NULL);
}

// Function to continuously receive messages from the server
void *receive_messages(void *arg)
{
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int valread;

    while (1)
    {

        char buffer[BUFFER_SIZE];
        int read_bytes = read(sock, buffer, BUFFER_SIZE - 1);

        // printf("Server response from another server: %s\n", buffer);

        if (read_bytes > 0)
        {
            buffer[read_bytes] = '\0'; // Null-terminate the received data

            // Check if the received message starts with PRIVMSG
            if (strncmp(buffer, "PRIVMSG", strlen("PRIVMSG")) == 0)
            {
                char *params[4]; // Array to hold command and parameters
                int num_params = 0;

                // Tokenize the buffer to extract parameters
                char *token = strtok(buffer, " ");
                while (token != NULL && num_params < 4)
                {
                    params[num_params++] = token;
                    token = strtok(NULL, " ");
                }

                // for (int i = 0; i < 4; i++)
                // {
                //     printf("params[%d]: %s\n", i, params[i]);
                // }

                // Check if num_params is correct for PRIVMSG command
                if (num_params == 4)
                {
                    // Call function to handle PRIVMSG command
                    handle_server_privmsg(params, num_params);
                }
            }
            else
            {
                buffer[read_bytes] = '\0'; // Null-terminate the received data
                // Handle other received messages
                printf("Server response: %s\n", buffer);
            }
        }
    }

    return NULL;
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    char *params[10]; // Array to hold command and parameters

    // Get client's IP address
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Client IP: %s\n", client_ip);
    }
    else
    {
        printf("Error getting client IP\n");
    }

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int read_bytes = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (read_bytes <= 0)
        {
            printf("Client %s disconnected.\n", client_ip);
            break; // Exit the loop if client disconnected
        }

        //printf("Received message from client: %s\n", buffer);
        int num_params = 0;
        char *token = strtok(buffer, " \r\n");
        while (token != NULL && num_params < 10)
        {
            params[num_params++] = token;
            token = strtok(NULL, " \r\n");
        }
        //printf(" numparams val : %d", num_params);
        // for (int i = 0; i < 10; i++) {
        //     printf("params[%d]: %s\n", i, params[i]);
        // }

        if (num_params == 0)
            continue;

        char *command = params[0];

        if (!command)
        {
            continue;
        }

        if (strcmp(command, "JOIN") == 0)
        {
            char *channelName = params[1];
            if (channelName)
            {
                join_channel(client_socket, channelName);
            }
            else
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
        }
        else if (strcmp(command, "NJOIN") == 0)
        {
            char *channelName = params[1];
            if (channelName)
            {
                handle_server_njoin(client_socket, params, num_params);
            }
            else
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
        }
        else if (strcmp(command, "PART") == 0)
        {
            char *channelName = strtok(NULL, " ");
            if (channelName)
            {
                part_channel(client_socket, channelName);
            }
            else
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
        }
        else if (strcmp(command, "TOPIC") == 0)
        {
            char *channelName = strtok(NULL, " ");
            char *topic = strtok(NULL, "\r\n"); // Assuming the topic ends with CRLF
            set_or_get_topic(client_socket, channelName, topic);
        }
        else if (strcmp(command, "PASS") == 0)
        {
            if (num_params == 2)
            {
                handle_pass_command(client_socket, params[1]); // Old simple PASS command
            }
            else if (num_params >= 3)
            {
                handle_server_pass(client_socket, params, num_params); // New extended PASS command
            }
            else
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
        }
        else if (strcmp(command, "NAMES") == 0)
        {
            char *channelName = strtok(NULL, " ");
            list_names(client_socket, channelName);
        }
        else if (strcmp(command, "NICK") == 0)
        {
            if (num_params == 3)
            {
                handle_nick_command(client_socket, params, num_params); // Existing simple NICK command
            }
            else if (num_params == 2)
            {
                handle_server_nick(client_socket, params, num_params); // New extended NICK command
               
            }
            else
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
        }
        else if (strcmp(command, "USER") == 0)
        {
            char *nickname = strtok(NULL, " ");
            char *realname = strtok(NULL, ":\r\n"); // Changed delimiter to ':' and added '\r\n' to end
            if (nickname && realname)
            {
                handle_user_command(client_socket, nickname, realname);
            }
            else
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
        }
        else if (strcmp(command, "SQUIT") == 0)
        {
            printf("Sever %s quit.\n", client_ip);
            char response[BUFFER_SIZE];
            strcpy(response, "BYE");
            send(client_socket, response, strlen(response), 0);
            break; // Exit the loop if client sends QUIT command
        }
        else if ((strcmp(command, "SQUIT") != 0) && ((strstr(command, "QUIT") != NULL) || (strcmp(command, "QUIT") == 0)))
        {
            printf("Client %s quit.\n", client_ip);
            char response[BUFFER_SIZE];
            strcpy(response, "BYE");
            send(client_socket, response, strlen(response), 0);
            break; // Exit the loop if client sends QUIT command
        }
        
        else if (strcmp(command, "PRIVMSG") == 0)
        {
            // for (int i = 0; i < 10; i++)
            // {
            //     printf("params[%d]: %s\n", i, params[i]);
            // }

            if (num_params < 3)
            {
                send_error(client_socket, ERR_NEEDMOREPARAMS);
            }
            else if (num_params == 3)
            {
            
                handle_privmsg(client_socket, params[1], params[2]);
            }
            else if (num_params == 4)
            {
           
                handle_server_privmsg(params, num_params);
            }
            else
            {
           
                send_error(client_socket, ERR_NORECIPIENT);
            }
        }
        else if (strstr(command, "TIME") != NULL)
        {
            char *target = strtok(NULL, " ");
            handle_time_command(client_socket);
        }
        else
        {
            printf("Unknown command from client: %s\n", command);
        }
    }

    close(client_socket);
    pthread_exit(NULL);
}

void handle_nick_command(int client_socket, char *params[], int num_params)
{
    if (num_params <2)
    {
        send_error(client_socket, ERR_NEEDMOREPARAMS);
        return;
    }

    char *nickname = params[1];
    char *password = params[2];

    // Trim the leading ':' from password if present
    if (password[0] == ':')
    {
        password++; // Skip the colon
    }
    // Check if the nickname is already in use
    int nick_in_use = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(user_info[i].nickname, nickname) == 0)
        {
            nick_in_use = 1;
            break;
        }
    }

    if (!nick_in_use)
    {
        int current_index = -1;
        // iterste in a for loop and find where the socket for nick is in the array and store the nickname at that index
        int index;
        for (index = 0; index < MAX_CLIENTS; index++)
        {
            if (user_info[index].client_socket == client_socket)
            {
                current_index = index;
                // Match found, copy the nickname and break out of the loop
                strncpy(user_info[index].nickname, nickname, MAX_NICK_LENGTH - 1);
                user_info[index].nickname[MAX_NICK_LENGTH - 1] = '\0'; // Ensure null-termination
                break;
            }
        }

        if (current_index == -1)
        {
            printf("User not found for socket %d.\n", client_socket);
            send_error(client_socket, ERR_NOSUCHNICK);
            return;
        }

        // if (strcmp(user_info[current_index].password, password) != 0)
        // {
        //     send_error(client_socket, ERR_INCORRECT_PASS); // You might consider creating a specific error for password mismatch
        //     return;
        // }

        int pass_found  = 0;
        for (int j = 0; j < MAX_CLIENTS; j++)
        {

            if (strcmp(user_info[j].password, password) == 0)
            {
                pass_found = 1;
                break;
            }
        }

        if(!pass_found){
            send_error(client_socket, ERR_INCORRECT_PASS); // You might consider creating a specific error for password mismatch
            return;
        }

        if (index == MAX_CLIENTS)
        {
            // Handle case when client_socket is not found in the array
            printf("Client socket not found in the array.\n");
        }
        printf("Client %d set nickname to %s\n", client_socket, user_info[index].nickname);
        // Send RPL_NICK
        char reply_msg[BUFFER_SIZE];
        snprintf(reply_msg, BUFFER_SIZE, ":%s 401 %s %s :Nickname is now %s\n", NICK, user_info[index].nickname, user_info[index].nickname, user_info[index].nickname);
        write(client_socket, reply_msg, strlen(reply_msg));
    }
    else
    {
        // Send ERR_NICKNAMEINUSE
        char err_msg[BUFFER_SIZE];
        snprintf(err_msg, BUFFER_SIZE, ":%s 433 %s %s :Nickname is already in use\n", NICK, user_info[client_socket].nickname, user_info[client_socket].nickname);
        write(client_socket, err_msg, strlen(err_msg));
    }
}

void handle_user_command(int client_socket, const char *nickname, const char *realname)
{
    int client_index = -1;

    // Find the index of user_info for the given client_socket
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (user_info[i].client_socket == client_socket)
        {
            client_index = i;
            break;
        }
    }

    // Check if the nickname is already set for any other client
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(user_info[i].nickname, nickname) == 0 && user_info[i].client_socket != client_socket)
        {
            // Nickname is already set for a different client, send ERR_NICKNAMEINUSE
            char err_msg[BUFFER_SIZE];
            snprintf(err_msg, BUFFER_SIZE, ":%s 433 %s %s :Nickname is already in use\n", NICK, user_info[i].nickname, user_info[i].nickname);
            write(client_socket, err_msg, strlen(err_msg));
            return;
        }
    }

    if (client_index != -1 && strlen(user_info[client_index].nickname) > 0)
    {
        // Nickname is already set, send RPL_WELCOME directly without registering again
        // Send RPL_WELCOME
        char welcome_msg[BUFFER_SIZE];
        snprintf(welcome_msg, BUFFER_SIZE, ":%s 001 %s :Welcome to the IRC network %s!%s@%s\n", NICK, user_info[client_index].nickname, user_info[client_index].nickname, user_info[client_index].nickname, NICK);
        write(client_socket, welcome_msg, strlen(welcome_msg));
    }
    else
    {
        // Nickname not set or client not found, proceed with registration
        if (client_index != -1)
        {
            // Store user information
            strncpy(user_info[client_index].nickname, nickname, MAX_NICK_LENGTH - 1);
            user_info[client_index].nickname[MAX_NICK_LENGTH - 1] = '\0';
            strncpy(user_info[client_index].realname, realname, BUFFER_SIZE - 1);
            user_info[client_index].realname[BUFFER_SIZE - 1] = '\0';

            // Send RPL_WELCOME
            char welcome_msg[BUFFER_SIZE];
            snprintf(welcome_msg, BUFFER_SIZE, ":%s 001 %s :Welcome to the IRC network %s!%s@%s\n", NICK, user_info[client_index].nickname, user_info[client_index].nickname, user_info[client_index].nickname, NICK);
            write(client_socket, welcome_msg, strlen(welcome_msg));
        }
        else
        {
            // Handle case when client_socket is not found in the array
            printf("Client socket not found in the array.\n");
        }
    }
}

void join_channel(int client_socket, const char *channelName)
{
    char response[BUFFER_SIZE];

    if (channelName == NULL || strlen(channelName) == 0)
    {
        send_error(client_socket, ERR_NEEDMOREPARAMS);
        return;
    }

    if (channelCount >= MAX_CHANNELS)
    {
        send_error(client_socket, ERR_TOOMANYCHANNELS);
        return;
    }

    if (strcmp(channelName, "0") == 0)
    {
        // User requested to leave all channels
        for (int i = 0; i < channelCount; i++)
        {
            for (int j = 0; j < channels[i].clientCount; j++)
            {
                if (channels[i].clients[j] == client_socket)
                {
                    // Remove client from channel
                    for (int k = j; k < channels[i].clientCount - 1; k++)
                    {
                        channels[i].clients[k] = channels[i].clients[k + 1];
                    }
                    channels[i].clientCount--;
                    break;
                }
            }
        }

        snprintf(response, BUFFER_SIZE, "You have left all channels.\n");
        send(client_socket, response, strlen(response), 0);
        return;
    }

    printf("Client %d attempting to join channel %s\n", client_socket, channelName);
    int found = 0;
    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channelName) == 0)
        {
            found = 1;
            if (channels[i].clientCount >= MAX_CLIENTS)
            {
                send_error(client_socket, ERR_CHANNELISFULL);
                return;
            }
            channels[i].clients[channels[i].clientCount++] = client_socket;
            snprintf(response, BUFFER_SIZE, "Client %d joined channel %s\n", client_socket, channelName);
            send(client_socket, response, strlen(response), 0);
            return;
        }
    }

    if (!found)
    {
        Channel newChannel;
        strncpy(newChannel.channelName, channelName, CHANNEL_NAME_LEN);
        newChannel.channelName[CHANNEL_NAME_LEN - 1] = '\0';
        newChannel.clients[0] = client_socket;
        newChannel.clientCount = 1;
        channels[channelCount++] = newChannel;
        snprintf(response, BUFFER_SIZE, "Channel %s created and client %d joined.\n", channelName, client_socket);
        send(client_socket, response, strlen(response), 0);
    }
}

void part_channel(int client_socket, const char *channelName)
{
    char response[BUFFER_SIZE];

    if (channelName == NULL || strlen(channelName) == 0)
    {
        send_error(client_socket, ERR_NEEDMOREPARAMS);
        return;
    }

    printf("Client %d attempting to leave channel %s\n", client_socket, channelName);
    int found = 0;
    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channelName) == 0)
        {
            found = 1;
            int clientFound = 0;
            for (int j = 0; j < channels[i].clientCount; j++)
            {
                if (channels[i].clients[j] == client_socket)
                {
                    for (int k = j; k < channels[i].clientCount - 1; k++)
                    {
                        channels[i].clients[k] = channels[i].clients[k + 1];
                    }
                    channels[i].clientCount--;
                    snprintf(response, BUFFER_SIZE, "Client %d left channel %s\n", client_socket, channelName);
                    send(client_socket, response, strlen(response), 0);
                    clientFound = 1;
                    break;
                }
            }
            if (!clientFound)
            {
                send_error(client_socket, ERR_NOTONCHANNEL);
            }
            break;
        }
    }

    if (!found)
    {
        send_error(client_socket, ERR_NOSUCHCHANNEL);
    }
}

void set_or_get_topic(int client_socket, const char *channelName, const char *topic)
{
    char response[BUFFER_SIZE];

    if (channelName == NULL || strlen(channelName) == 0)
    {
        send_error(client_socket, ERR_NEEDMOREPARAMS);
        return;
    }

    int found = 0;
    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channelName) == 0)
        {
            found = 1;
            if (topic == NULL)
            {
                if (strlen(channels[i].topic) > 0)
                {
                    snprintf(response, BUFFER_SIZE, "Topic for %s is %s\n", channelName, channels[i].topic);
                    send(client_socket, response, strlen(response), 0);
                }
                else
                {
                    send_error(client_socket, RPL_NOTOPIC);
                }
            }
            else
            {
                strncpy(channels[i].topic, topic, sizeof(channels[i].topic) - 1);
                channels[i].topic[sizeof(channels[i].topic) - 1] = '\0';
                snprintf(response, BUFFER_SIZE, "Topic for %s set to %s\n", channelName, topic);
                send(client_socket, response, strlen(response), 0);
            }
            return;
        }
    }

    if (!found)
    {
        send_error(client_socket, ERR_NOSUCHCHANNEL);
    }
}

void list_names(int client_socket, const char *channelName)
{
    char response[BUFFER_SIZE * MAX_CLIENTS]; // Make sure this buffer is large enough

    int found = 0;
    if (channelName == NULL)
    {
        strcpy(response, "Listing all channels and users:\n");
        for (int i = 0; i < channelCount; i++)
        {
            snprintf(response + strlen(response), sizeof(response) - strlen(response), "Channel: %s\n", channels[i].channelName);
            for (int j = 0; j < channels[i].clientCount; j++)
            {
                for (int k = 0; k < MAX_CLIENTS; k++)
                {
                    if (user_info[k].client_socket == channels[i].clients[j])
                    {
                        snprintf(response + strlen(response), sizeof(response) - strlen(response), "User: %s\n", user_info[k].nickname);
                        break;
                    }
                }
            }
        }
        send(client_socket, response, strlen(response), 0);
    }
    else
    {
        for (int i = 0; i < channelCount; i++)
        {
            if (strcmp(channels[i].channelName, channelName) == 0)
            {
                found = 1;
                snprintf(response, sizeof(response), "Channel: %s\n", channels[i].channelName);
                for (int j = 0; j < channels[i].clientCount; j++)
                {
                    for (int k = 0; k < MAX_CLIENTS; k++)
                    {
                        if (user_info[k].client_socket == channels[i].clients[j])
                        {
                            snprintf(response + strlen(response), sizeof(response) - strlen(response), "User: %s\n", user_info[k].nickname);
                            break;
                        }
                    }
                }
                break;
            }
        }

        if (!found)
        {
            send_error(client_socket, ERR_NOSUCHCHANNEL);
        }
        else
        {
            send(client_socket, response, strlen(response), 0);
        }
    }
}

void handle_privmsg(int client_socket, const char *msgtarget, const char *message)
{
    char response[BUFFER_SIZE];
    remove_extra_spaces(msgtarget);

    printf("Handling PRIVMSG: target=%s, message=%s\n", msgtarget, message);

    if (!msgtarget || !message)
    {
        printf("here1");
        send_error(client_socket, ERR_NORECIPIENT);
        return;
    }

    if (strlen(message) == 0)
    {
        send_error(client_socket, ERR_NOTEXTTOSEND);
        return;
    }

    // Assume msgtarget can be a nickname or channel
    int found = 0;
    if (msgtarget[0] == '#')
    { // Channel message
        for (int i = 0; i < channelCount; i++)
        {
            printf("Checking channel: %s\n", channels[i].channelName);
            if (strcmp(channels[i].channelName, msgtarget) == 0)
            {
                found = 1;
                printf("Channel found: %s\n", msgtarget);
                for (int j = 0; j < channels[i].clientCount; j++)
                {
                    if (channels[i].clients[j] != client_socket)
                    {
                        snprintf(response, BUFFER_SIZE, "%s: %s\n", msgtarget, message);
                        printf("Sending message to client %d in channel %s\n", channels[i].clients[j], msgtarget);
                        send(channels[i].clients[j], response, strlen(response), 0);
                    }
                }
                break;
            }
        }
        if (!found)
        {
            send_error(client_socket, ERR_NOSUCHCHANNEL);
        }
    }
    else
    { // Private message to a user
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            remove_extra_spaces(user_info[i].nickname);
            printf("Checking nickname: %s with msgtarget %s\n", user_info[i].nickname, msgtarget);
            if (strcmp(user_info[i].nickname, msgtarget) == 0)
            {
                found = 1;
                printf("Nickname found: %s with socket %d\n", msgtarget, user_info[i].client_socket);
                snprintf(response, BUFFER_SIZE, "From %s: %s\n", user_info[client_socket].nickname, message);
                // printf("Sending private message to %s (socket %d)\n", msgtarget, user_info[i].client_socket);
                send(user_info[i].client_socket, response, strlen(response), 0);
                printf("Sending private message to %s (socket %d)\n", msgtarget, user_info[i].client_socket);
                break;
            }
        }
        if (!found)
        {
            send_error(client_socket, ERR_NOSUCHNICK);
        }
    }
}

void remove_extra_spaces(char *str)
{
    int i, j;
    int length = strlen(str);
    int space_flag = 0; // Flag to track if space was encountered

    // Iterate through the string
    for (i = 0, j = 0; i < length; i++)
    {
        // Skip newline characters
        if (str[i] == '\n')
        {
            continue;
        }
        // If current character is not whitespace or if space_flag is not set
        if (!isspace((unsigned char)str[i]) || !space_flag)
        {
            str[j++] = str[i];                           // Copy the character to the new position
            space_flag = isspace((unsigned char)str[i]); // Update space_flag
        }
    }
    str[j] = '\0'; // Null-terminate the modified string
}

void handle_time_command(int client_socket)
{
    char response[BUFFER_SIZE];
    time_t now;
    struct tm *local_time;

    time(&now);
    local_time = localtime(&now);

    // No target specified or target is this server, send the local time
    strftime(response, BUFFER_SIZE, ":%s 391 :Local time is %H:%M:%S\n", local_time);
    send(client_socket, response, strlen(response), 0);
}

void handle_server_nick(int client_socket, char *params[], int num_params)
{

    if (num_params <2)
    {
        send_error(client_socket, ERR_NEEDMOREPARAMS);
        return;
    }

    char *nickname = params[1];


   
    // Check if the nickname is already in use
    int nick_in_use = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (strcmp(user_info[i].nickname, nickname) == 0)
        {
            nick_in_use = 1;
            break;
        }
    }

    if (!nick_in_use)
    {
        int current_index = -1;
        // iterste in a for loop and find where the socket for nick is in the array and store the nickname at that index
        int index;
        for (index = 0; index < MAX_CLIENTS; index++)
        {
            if (user_info[index].client_socket == client_socket)
            {
                current_index = index;
                // Match found, copy the nickname and break out of the loop
                strncpy(user_info[index].nickname, nickname, MAX_NICK_LENGTH - 1);
                user_info[index].nickname[MAX_NICK_LENGTH - 1] = '\0'; // Ensure null-termination
                break;
            }
        }

        
        printf("Sever %d set nickname to %s\n", client_socket, user_info[index].nickname);
        // Send RPL_NICK
        char reply_msg[BUFFER_SIZE];
        snprintf(reply_msg, BUFFER_SIZE, ":%s 401 %s %s :Nickname is now %s\n", NICK, user_info[index].nickname, user_info[index].nickname, user_info[index].nickname);
        write(client_socket, reply_msg, strlen(reply_msg));
    }
    else
    {
        // Send ERR_NICKNAMEINUSE
        char err_msg[BUFFER_SIZE];
        snprintf(err_msg, BUFFER_SIZE, ":%s 433 %s %s :Nickname is already in use\n", NICK, user_info[client_socket].nickname, user_info[client_socket].nickname);
        write(client_socket, err_msg, strlen(err_msg));
    }
    
}

void handle_server_pass(int client_socket, char *params[], int num_params)
{
    if (num_params < 2)
    {
        printf("Error: PASS command requires at least 1 parameter.\n");
        return;
    }
    // Extract parameters
    char *password = params[1];
    // Optional parameters are ignored for clients, considered for servers
    char *version = (num_params > 2) ? params[2] : NULL;
    char *flags = (num_params > 3) ? params[3] : NULL;
    char *options = (num_params > 4) ? params[4] : NULL;

    // Log for debugging
    // printf("Received PASS command with password: %s\n", password);
    if (version)
        printf("Version: %s\n", version);
    if (flags)
        printf("Flags: %s\n", flags);
    if (options)
        printf("Options: %s\n", options);

    // Assuming serverConfig is an array of ServerInfo and user_info is properly declared
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(client_socket, (struct sockaddr *)&addr, &addr_len) != 0)
    {
        perror("Error getting client socket address");
        return;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    // Find or create user info
    UserInfo *user = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (user_info[i].client_socket == client_socket)
        {
            user = &user_info[i];
            break;
        }
    }

    if (!user)
    {
        // Handle new connection case
        printf("New server/client connection\n");
    }
    else
    {
        
        printf("Password verified for returning client/server IP %s.\n", client_ip);
    }

    // Store the password for new or unverified connections
    if (user)
    {
        strncpy(user->password, password, BUFFER_SIZE - 1);
        user->password[BUFFER_SIZE - 1] = '\0';
    }

    printf("Password set/updated for client IP %s.\n", client_ip);
}

// Giving but msg BYE but need to gracefully exit and terminate the code

void handle_server_squit(int client_socket, char *params[], int num_params)
{
    if (num_params < 2)
    {
        printf("Error: SQUIT command requires at least 1 parameter for the server name.\n");
        return;
    }

    char *server_name = params[1];
    char *comment = (num_params > 2) ? params[2] : "No comment provided";

    printf("Handling server SQUIT command. Server: %s, Comment: %s\n", server_name, comment);

    // Broadcast the SQUIT message to all connected servers
    broadcast_squit(server_name, comment);

    // Close all client and server connections
    shutdown_all_connections();

    // Shutdown the server
    printf("Shutting down the server gracefully.\n");
    exit(0);
}

void handle_server_njoin(int client_socket, char *params[], int num_params)
{
    if (num_params != 2)
    {
        printf("Error: NJOIN command requires exactly 1 parameter.\n");
        return;
    }

    char *channel_name = params[1];

    // Search for the channel in the channel list
    Channel *channel = NULL;
    for (int i = 0; i < channelCount; i++)
    {
        if (strcmp(channels[i].channelName, channel_name) == 0)
        {
            channel = &channels[i];
            break;
        }
    }

    // If the channel doesn't exist, create a new one
    if (channel == NULL)
    {
        if (channelCount >= MAX_CHANNELS)
        {
            printf("Error: Maximum number of channels reached.\n");
            return;
        }

        channel = &channels[channelCount];
        strncpy(channel->channelName, channel_name, CHANNEL_NAME_LEN - 1);
        channel->channelName[CHANNEL_NAME_LEN - 1] = '\0'; // Null-terminate
        channel->clientCount = 0;
        strcpy(channel->topic, ""); // Set an empty topic initially
        channelCount++;
        printf("Created and joined channel '%s'.\n", channel_name);
    }
    else
    {
        // Join the existing channel
        // Add client to the channel's clients array
        printf("Joined channel '%s'.\n", channel_name);
    }
}

void handle_server_privmsg(char *params[], int num_params)
{
    if (num_params != 4)
    {
        printf("Error: PRIVMSG command requires exactly 4 parameters.\n");
        return;
    }

    char *target_server_ip_addr = params[1];

    // Check if the target server IP is not equal to any server IP in ServerAddress array
    int found_target_server = 1;
    for (int i = 0; i < server_info.serverCount; i++)
    {
        if (strcmp(target_server_ip_addr, server_info.nick) == 0)
        {
            found_target_server = 0;
            break;
        }
    }

    //printf("/n found target : %d", found_target_server);
    // Check if the target server IP is equal to the current server's IP address
    if (found_target_server == 0)
    {
        // The target server IP matches the current server's IP
        char *target_nick = params[2];
        char *message = params[3];

        // print recieved params for debugging
        // printf(params);
        // Search for the target nick in user_info array
        int target_client_socket = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            remove_extra_spaces(user_info[i].nickname);
            if (strcmp(target_nick, user_info[i].nickname) == 0)
            {
                target_client_socket = user_info[i].client_socket;
                break;
            }
        }

        if (target_client_socket != -1)
        {
            // Send the message to the target client socket
            send(target_client_socket, message, strlen(message), 0);
        }
        // else
        // {
        //     // Target nick not found in user_info array, send error to client
        //     send_error(client_socket, ERR_NOSUCHNICK);
        // }
    }
    else
    {

        char full_message[BUFFER_SIZE];

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            remove_extra_spaces(user_info[i].nickname);
            // printf("Checking nickname: %s with msgtarget %s\n", user_info[i].nickname, msgtarget);
            if (strcmp(user_info[i].nickname, target_server_ip_addr) == 0)
            {
                // printf("Nickname found: %s with socket %d\n", target_server_ip_addr, user_info[i].client_socket);
                snprintf(full_message, BUFFER_SIZE, "%s %s %s %s", params[0], params[1], params[2], params[3]);
                // printf("Sending private message to %s (socket %d)\n", msgtarget, user_info[i].client_socket);
                send(user_info[i].client_socket, full_message, strlen(full_message), 0);
                // printf("Sending private message to %s (socket %d)\n", msgtarget, user_info[i].client_socket);
                break;
            }
        }

        // Send the message to the target server's server_port

    }
}

void broadcast_squit(char *server_name, char *comment)
{
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "SQUIT %s :%s", server_name, comment);

    for (int i = 0; i < MAX_CONNECTED_SERVERS; i++)
    {
        if (serverConfig[i].client_port > 0)
        { // Assuming there's a field to check if the socket is active
            send(serverConfig[i].client_port, message, strlen(message), 0);
            printf("Sent SQUIT to %s (%s)\n", serverConfig[i].nick, serverConfig[i].sockAddr[0].ip);
        }
    }
}
void shutdown_all_connections()
{
    // Close all client connections
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (user_info[i].client_socket != 0)
        {
            close(user_info[i].client_socket);
            user_info[i].client_socket = 0; // Mark as closed
            printf("Closed client connection socket %d\n", user_info[i].client_socket);
        }
    }
}
// Use remove_spce function
void handle_pass_command(int client_socket, const char *password)
{
    // Check if the client has already registered (simplified by checking nickname or some registration flag)
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (user_info[i].client_socket == client_socket)
        {
            // Check if user has already been registered
            if (strlen(user_info[i].nickname) > 0)
            {
                printf("Error: User has already registered. PASS must be set before NICK/USER.\n");
                return; // Do not overwrite or set password if already registered
            }

            // Store the password for new or unregistered users
            strncpy(user_info[i].password, password, BUFFER_SIZE - 1);
            user_info[i].password[BUFFER_SIZE - 1] = '\0';
            printf("Password received and stored for client %d\n", client_socket);

            // Optionally, save the password to a file
            save_password_to_file(client_socket, password);
            return;
        }
    }
    printf("Error: Client socket %d not found in user_info array.\n", client_socket);
}

void save_password_to_file(int client_socket, const char *password)
{
    FILE *file = fopen(".usr_pass", "a"); // Open the file in append mode
    if (file == NULL)
    {
        perror("Failed to open password file");
        return;
    }
    fprintf(file, "%d:%s\n", client_socket, password);
    fclose(file);
    printf("Password for socket %d saved successfully in .usr_pass file.\n", client_socket);
}
