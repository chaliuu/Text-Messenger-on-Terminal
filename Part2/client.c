#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdbool.h>

#define MAX_NAME 1024
#define MAX_DATA 1024

enum command_type {
    LOGIN, LO_ACK, LO_NAK, EXIT, JOIN, JN_ACK, JN_NAK, LEAVE_SESS, NEW_SESS, NS_ACK, MESSAGE, QUERY, QU_ACK, 
    PRIVATE_MESSAGE, PM_ACK, PM_NAK, REGISTER, REG_ACK, REG_NAK
};

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

  int sockfd = -1; //Socket file descriptor
    bool isLoggedIn;
    bool isInSesh;
    bool isQuit;
    bool isConnected = false;
    char IP[256];
    char PORT[256];
    char cID[MAX_NAME] = {0};
    char sID[MAX_DATA] = {0};
    struct message msg; //message to send from input
    struct message recv_msg; //message recieved from server

    struct timeval tv;
    int retval;

    char input[MAX_DATA + MAX_NAME] = {0};
    char send_buffer[MAX_DATA + MAX_NAME + 20]; // +20 for type, size, and separators
    char recv_buffer[MAX_DATA + MAX_NAME] = {0};

void send_message(struct message msg);
int parse_command(char *input);
void serialize_message(struct message msg, char *output);
void deserialize_message(char *input, struct message *msg);
int connect_to_server(char *ip, char* port);

int main(int argc, char const *argv[]) {
    isQuit = false;
    fd_set readfds;
    // Main loop for handling user input
    while (!isQuit) {
        //wipe buffer
        memset(&input, 0, MAX_DATA + MAX_NAME);
        memset(&send_buffer, 0, MAX_DATA + MAX_NAME);
        memset(&recv_buffer, 0, MAX_DATA + MAX_NAME);

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // Standard input
        FD_SET(sockfd, &readfds);
        if (sockfd != -1) {
            FD_SET(sockfd, &readfds);
        }

        // Set timeout (for 5 minutes)
        tv.tv_sec = 300;
        tv.tv_usec = 0;

        retval = select((sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO) + 1, &readfds, NULL, NULL, &tv);
        if(retval == -1){
            perror("select error");
            exit(1);
        } else if (retval){
            //check stdin input
            if(FD_ISSET(STDIN_FILENO, &readfds)) {
                if(fgets(input, sizeof(input), stdin) != NULL){
                    input[strcspn(input, "\n")] = 0; // Remove trailing newline
                    parse_command(input);
                }
                if (msg.type == LOGIN){
                    
                    if (connect_to_server(IP, PORT) != 0) {
                        printf("Failed to connect to the server.\n");
                        continue;
                    }

                    send_message(msg);

                }else if (msg.type == REGISTER){
                    printf("...registering...\n");
                    printf("IP: %s\n", IP);
                    printf("PORT: %s\n", PORT);
                    if (connect_to_server(IP, PORT) != 0) {
                        printf("Failed to connect to the server.\n");
                        continue;
                    }
                    send_message(msg);
        
                }else if (msg.type == EXIT){
                    send_message(msg);
                    memset(cID,0,sizeof(cID)); //clear cID
                }else if (sockfd != -1 && isLoggedIn) {
                // Only send messages if logged in (i.e., socket is valid)
                    send_message(msg);
                }else if (sockfd != -1 && !isLoggedIn){
                    printf("Not logged in. Please log in\n");
                }
                else {
                    printf("You are not connected to a server. Please log in first.\n");
                }
                if (msg.type == EXIT && isQuit) {
                    printf("Exiting...\n");
                    break;
                }
            }
            
            //check messages from server
            if (sockfd != -1 && FD_ISSET(sockfd, &readfds)) {
                int numBytes = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
                if (numBytes > 0) {
                    memset(&recv_msg, 0, sizeof(recv_msg));
                    deserialize_message(recv_buffer, &recv_msg);
                    if(recv_msg.type == MESSAGE){
                        //printf("%s", recv_buffer);
                        printf("%s: %s\n", recv_msg.source, recv_msg.data); //print received messages

                    }else if(recv_msg.type == PM_ACK){
                        //printf("%s", recv_buffer);
                        printf("PRIVATE MESSAGE FROM %s: %s\n", recv_msg.source, recv_msg.data); //print received private messages

                    }else if (recv_msg.type == LO_NAK){
                        printf("ERROR: %s\n", recv_msg.data);
                        isLoggedIn = false;
                    }
                    else if (recv_msg.type == JN_NAK){
                        printf("ERROR: %s\n", recv_msg.data);
            
                    }else if (recv_msg.type == LO_ACK){
                        printf("Sucessfully logged in!\n");

                    }else if (recv_msg.type == JN_ACK){
                        isInSesh = true;
                        strcpy(sID, (char *)msg.data);
                        printf("Session joined with ID: %s\n", recv_msg.data);

                    }else if (recv_msg.type == NS_ACK){
                        isInSesh = true;
                        strcpy(sID, (char *)msg.data);
                        printf("Session created!\n");

                    }else if (recv_msg.type == QU_ACK){
                        printf("/*%s*/\n", recv_msg.data);

                    }else if (recv_msg.type == REG_ACK){
                        printf("Sucessfully registered!\n");

                    }else if (recv_msg.type == REG_NAK){
                        printf("ERROR: %s\n", recv_msg.data);
                    }else if (recv_msg.type ==  PM_NAK){
                        printf("ERROR: %s\n", recv_msg.data);

                    }
                } else if (numBytes == 0){
                    printf("Connection closed by server\n");
                    close(sockfd);
                    sockfd = -1;
                } else if (numBytes < 0 && errno != EWOULDBLOCK) {
                    printf("Connection failed.\n");
                    close(sockfd);
                    sockfd = -1; // Reset sockfd to indicate we're not connected
                }
            }
        }else{
            printf("Are you still there?\n");
        }
    }

    return 0;
}

int connect_to_server(char *ip, char * port) {
    //references Beej's Programming Guide
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
        perror("getaddrinfo error!\n");
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
    memset(&msg, 0, sizeof(msg)); // Initialize/clear the message structure

    if(strncmp(input, "/login", 6) == 0) {
        msg.type = LOGIN;
        sscanf(input, "/login %s %s %s %s", msg.source, msg.data, IP, PORT); 
        strcpy(cID, (char *)msg.source);
        isLoggedIn = true;
    } else if(strncmp(input, "/logout", 8) == 0) {
        if(isLoggedIn){
            isLoggedIn = false;
            isInSesh = false;
            msg.type = EXIT;
            printf("Logged out of client with ID: %s\n", cID);
        }else{
            printf("Not logged In. Please log in before logging out\n");
        }
    } else if(strncmp(input, "/joinsession", 12) == 0) {
        if(isInSesh){
            printf("Already in seshion %s\n", sID);
        }else{
            sscanf(input, "/joinsession %s", msg.data);
            msg.type = JOIN;
            strcpy((char *)msg.source, cID);
        }
    } else if(strncmp(input, "/leavesession", 13) == 0) {
        if(isInSesh){
            msg.type = LEAVE_SESS;
            isInSesh = false;
            printf("Left Session with ID, %s\n", sID);
            memset(sID,0,sizeof(sID)); //clear sID
        }else{
            printf("Not in a session. Please join a session before leaving one.\n");
        }
    } else if(strncmp(input, "/createsession", 14) == 0) {
        if(isInSesh){
            printf("Already in seshion %s\n. Leave session before creating another one.\n", sID);
        }else{
            msg.type = NEW_SESS;
        }
        sscanf(input, "/createsession %s", msg.data);
    } else if(strncmp(input, "/list", 5) == 0) {
        msg.type = QUERY;
    } else if(strncmp(input, "/quit", 5) == 0) {
        isLoggedIn = false;
        isInSesh = false;
        isConnected = false;
        msg.type = EXIT;
        isQuit = true;
        printf("Quiting Program!\n");
    } else if (strncmp(input, "/register", 9) == 0){   
        msg.type = REGISTER;
        sscanf(input, "/regsiter %s %s %s %s", msg.source, msg.data, IP, PORT); 
        strcpy(cID, (char *)msg.source);
    } else if (strncmp(input, "/pm", 3) == 0){
        msg.type = PRIVATE_MESSAGE;
        strncpy((char *)msg.data, input, MAX_DATA);
    }
    else {
        msg.type = MESSAGE;
        strncpy((char *)msg.data, input, MAX_DATA);
    }

    strcpy((char *)msg.source, cID);
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
