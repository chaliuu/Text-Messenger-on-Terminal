#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_NAME 32
#define MAX_DATA 1024

enum command_type {
    LOGIN, LO_ACK, LO_NAK, EXIT, JOIN, JN_ACK, JN_NAK, LEAVE_SESS, NEW_SESS, NS_ACK, MESSAGE, QUERY, QU_ACK
};

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

// Global variables for client state
int sock;
pthread_mutex_t lock;
pthread_t recv_thread;

// Function declarations
void *receive_handler(void *socket_desc);
void send_message(struct message msg);
struct message parse_command(char *input);
void serialize_message(struct message msg, char *output);
void deserialize_message(char *input, struct message *msg);

int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    char input[MAX_DATA + MAX_NAME], serializedMessage[MAX_DATA + MAX_NAME];
    struct message msg;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket\n");
        return -1;
    }

    server.sin_addr.s_addr = inet_addr("127.0.0.1"); // Placeholder, replace with actual server IP
    server.sin_family = AF_INET;
    server.sin_port = htons(8888); // Placeholder, replace with actual server port

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed");
        return 1;
    }

    printf("Connected to server\n");

    if(pthread_create(&recv_thread, NULL, receive_handler, (void*)&sock) < 0) {
        perror("Could not create thread");
        return 1;
    }

    // Main loop for handling user input
    while (1) {
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0; // Remove trailing newline

        msg = parse_command(input);
        serialize_message(msg, serializedMessage);
        send_message(msg);

        if (msg.type == EXIT) {
            printf("Exiting...\n");
            break;
        }
    }

    pthread_join(recv_thread, NULL);
    close(sock);

    return 0;
}

void *receive_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char server_reply[MAX_DATA + MAX_NAME];
    struct message msg;

    while(1) {
        if(recv(sock, server_reply, sizeof(server_reply), 0) > 0) {
            deserialize_message(server_reply, &msg);
            // Handle different types of messages here, for simplicity we just print
            printf("Received: %s\n", msg.data);
        }
    }
}

// Sends a message to the server
void send_message(struct message msg) {
    char buffer[MAX_DATA + MAX_NAME + 20]; // +20 for type, size, and separators
    serialize_message(msg, buffer);
    if(send(sock, buffer, strlen(buffer), 0) < 0) {
        puts("Send failed");
    }
}

// Parses user input into a message structure
struct message parse_command(char *input) {
    struct message msg;
    memset(&msg, 0, sizeof(msg)); // Initialize the message structure

    if(strncmp(input, "/login", 6) == 0) {
        msg.type = LOGIN;
        sscanf(input, "/login %s %s %s %d", msg.source, msg.data, &msg.size); // Assuming IP and port are in msg.data separated by space
    } else if(strcmp(input, "/logout") == 0) {
        msg.type = EXIT;
    } else if(strncmp(input, "/joinsession", 12) == 0) {
        msg.type = JOIN;
        sscanf(input, "/joinsession %s", msg.data);
    } else if(strcmp(input, "/leavesession") == 0) {
        msg.type = LEAVE_SESS;
    } else if(strncmp(input, "/createsession", 14) == 0) {
        msg.type = NEW_SESS;
        sscanf(input, "/createsession %s", msg.data);
    } else if(strcmp(input, "/list") == 0) {
        msg.type = QUERY;
    } else if(strcmp(input, "/quit") == 0) {
        msg.type = EXIT;
    } else {
        msg.type = MESSAGE;
        strncpy(msg.data, input, MAX_DATA);
    }

    msg.size = strlen(msg.data); // Set the size of the data
    return msg;
}

// Serializes a message structure into a string
void serialize_message(struct message msg, char *output) {
    sprintf(output, "%u:%u:%s:%s", msg.type, msg.size, msg.source, msg.data);
}

// Deserializes a string into a message structure
void deserialize_message(char *input, struct message *msg) {
    sscanf(input, "%u:%u:%[^:]:%[^:]", &msg->type, &msg->size, msg->source, msg->data);
}
