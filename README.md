

# RFC 2813 IRC Server Implementation

This repository contains an implementation of an IRC (Internet Relay Chat) server protocol based on [RFC 2813](https://datatracker.ietf.org/doc/html/rfc2813). The server supports core functionalities required for IRC servers, including user connections, authentication, command parsing, message relaying, channel management, and server-to-server communication.

## Features

This RFC 2813 IRC server implementation includes the following key features:

1. **Connection Handling**:
   - Accepts incoming client and server connections, validates each connection, and handles initial connection setup.

2. **Authentication**:
   - Authenticates users or servers based on predefined credentials. Valid users proceed to further registration, while failed attempts trigger error handling.

3. **User Registration**:
   - Registers users, ensuring each user has a unique nickname. Prompts users to choose new nicknames if there are conflicts.

4. **Command Parsing**:
   - Interprets IRC commands such as `JOIN`, `PART`, `PRIVMSG`, and `NICK`. Valid commands are routed to their respective processing modules, while invalid commands trigger error handling.

5. **Message Relay**:
   - Relays messages between clients within channels, across servers, and directly between users.

6. **Error Handling**:
   - Provides error codes and messages for failed connections, authentication errors, invalid commands, and other issues.

7. **Channel Management**:
   - Manages user interactions with IRC channels, including creating, joining, and parting channels with permissions and restrictions as necessary.

8. **Server-to-Server Communication**:
   - Supports server communication, allowing the relay of messages across connected IRC servers for network consistency.

9. **Logging and Monitoring**:
   - Logs key actions, errors, and events for system monitoring, helping in troubleshooting and system health analysis.

## Installation

### Prerequisites

- **Python 3.8+** (or the appropriate language version if another language is used)
- Required libraries, as specified in `requirements.txt`

### Steps

1. Clone this repository:
   ```bash
   git clone https://github.com/your-username/rfc2813-irc-server.git
   ```
2. Navigate to the project directory:
   ```bash
   cd rfc2813-irc-server
   ```
3. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

To start the IRC server, use the following command:

```bash
python irc_server.py
```

By default, the server will start on the predefined port and IP address specified in the configuration file. Modify the configuration file (`config.yaml` or similar) to adjust settings such as the server port, IP address, and connection limits.

### Connecting a Client

Use any IRC client (such as HexChat or WeeChat) to connect to the server. In the client, configure the connection using the server IP and port specified in the server configuration.

### Example Commands

Once connected, the following commands can be tested on the server:

- **JOIN**: Join an IRC channel.
  ```text
  JOIN #channel
  ```
- **PRIVMSG**: Send a private message to a user or channel.
  ```text
  PRIVMSG #channel :Hello everyone!
  PRIVMSG username :Hello user!
  ```
- **NICK**: Set or change nickname.
  ```text
  NICK newnickname
  ```
- **PART**: Leave a channel.
  ```text
  PART #channel
  ```

## Configuration

The server behavior can be customized through the configuration file, where you can modify:

- **Server Port and IP Address**: Customize the IP address and port on which the server listens for connections.
- **Authentication Settings**: Configure authentication details and user credentials.
- **Logging Level**: Adjust logging verbosity (INFO, DEBUG, ERROR).
- **Channel Limits**: Set maximum channel size and other restrictions.

## Workflow

The server follows a structured workflow from connection handling to error management:

1. **Connection Handling**: Validates client/server connections.
2. **Authentication**: Authenticates and verifies credentials.
3. **User Registration**: Registers and assigns nicknames to users.
4. **Command Parsing**: Parses and validates commands.
5. **Message Relay**: Routes messages between users and servers.
6. **Error Handling**: Manages errors at each stage.
7. **Channel Management**: Handles channel operations.
8. **Server-to-Server Communication**: Supports network consistency.

## Error Handling

The server uses structured error handling to manage issues that arise during connections, command parsing, or message relaying. Errors are logged and displayed to clients as needed.

## Logging and Monitoring

The server logs key actions, events, and errors for operational monitoring. All logs are stored in the `/logs` directory by default, with a configurable logging level.

## Contributing

Contributions are welcome! Please open an issue to discuss your idea or create a pull request with your changes.

### Guidelines

- Ensure all code follows the repository’s style guidelines.
- Include tests for any new functionality.
- Provide documentation for significant changes or new features.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## References

- [RFC 2813 - Internet Relay Chat: Server Protocol](https://datatracker.ietf.org/doc/html/rfc2813)
- [RFC 1459 - Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)

---

