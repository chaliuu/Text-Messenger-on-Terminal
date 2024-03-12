#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdbool.h>

#define MAX_NAME 1024
#define MAX_DATA 1024

enum command_type {
    LOGIN, LOGOUT, LO_ACK, LO_NAK, EXIT, JOIN, JN_ACK, JN_NAK, LEAVE_SESS, NEW_SESS, NS_ACK, MESSAGE, QUERY, QU_ACK
};

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

int sockfd = -1;
bool isLoggedIn;
pthread_t recv_thread;
char IP[128];
char PORT[128];
struct message msg;

char input[MAX_DATA + MAX_NAME];
char server_reply[MAX_DATA + MAX_NAME + 20];
char send_buffer[MAX_DATA + MAX_NAME + 20]; // +20 for type, size, and separators

void receive_handler();
void send_message(struct message msg);
int parse_command(char *input);
void serialize_message(struct message msg, char *output);
void deserialize_message(char *input, struct message *msg);
int connect_to_server(char *ip, char* port);

int main(int argc, char const *argv[]) {
    
    // Main loop for handling user input
    while (1) {
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0; // Remove trailing newline

        parse_command(input);

        if (msg.type == LOGIN) {
            if (connect_to_server(IP, PORT) != 0) {
                printf("Failed to connect to the server.\n");
                continue;
            }
        } else if (sockfd != -1 && isLoggedIn) {
            // Only send messages if logged in (i.e., socket is valid)
            send_message(msg);
        } else if (sockfd != -1 && !(isLoggedIn)){
            printf("Not logged in. Please log in");
        }
        else {
            printf("You are not connected to a server. Please log in first.\n");
        }

        if (msg.type == EXIT) {
            printf("Exiting...\n");
            break;
        }
    }

    return 0;
}

int connect_to_server(char *ip, char * port) {
    //references Beej's Programming Guide
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
        perror("getaddrinfo\n");
        exit(1);
    }
    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        printf("talker: failed to create socket\n");
     return 2;
    }
   if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        perror("talker: connect");
        exit(1);
    }
    
    freeaddrinfo(servinfo);
    return 0;

    /*
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket\n");
        return -1;
    }

    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed");
        return 1;
    }

    printf("Connected to server\n");

    if(pthread_create(&recv_thread, NULL, receive_handler, (void*)&sock) < 0) {
        perror("Could not create receive thread");
        return 1;
    }

    return 0;
    */
}

void receive_handler() {
    struct message msg;

    while(1) {
        if(recv(sockfd, server_reply, sizeof(server_reply), 0) > 0) {
            deserialize_message(server_reply, &msg);
            // Handle different types of messages here, for simplicity we just print
            printf("Received: %s\n", msg.data);
        }
    }
}

// Sends a message to the server
void send_message(struct message msg) {
    serialize_message(msg, send_buffer);
    if(send(sockfd, send_buffer, strlen(send_buffer), 0) < 0) {
        puts("Send failed");
    }
}

// Parses user input into a message structure
int parse_command(char *input) {
    memset(&msg, 0, sizeof(msg)); // Initialize the message structure

    if(strncmp(input, "/login", 6) == 0) {
        msg.type = LOGIN;
        sscanf(input, "/login %s %s %s %s", msg.source, msg.data, IP, PORT); 
    } else if(strcmp(input, "/logout") == 0) {
        isLoggedIn = false;
        msg.type = LOGOUT;
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
        strncpy((char *)msg.data, input, MAX_DATA);
    }

    msg.size = strlen((char *)msg.data); // Set the size of the data
    return 0;
}

// Serializes a message structure into a string
void serialize_message(struct message msg, char *output) {
    sprintf(output, "%u:%u:%s:%s", msg.type, msg.size, msg.source, msg.data);
}

// Deserializes a string into a message structure
void deserialize_message(char *input, struct message *msg) {
    sscanf(input, "%u:%u:%[^:]:%[^:]", &msg->type, &msg->size, msg->source, msg->data);
}