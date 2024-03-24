/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 * Click nbfs://nbhost/SystemFileSystem/Templates/cFiles/main.c to edit this template
 */

/* 
 * File:   server.c
 * Author: wengchan
 *
 * Created on March 9, 2024, 11:35 PM
 */
#include "packet.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 20
/*
 * 
 */

#define LOGIN_FILE "login.txt"

struct CLIENT_NODE* client_head = NULL;
struct SESSION_NODE* session_head = NULL;

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



int main(int argc, char** argv) {
    // process command line input
    if (argc != 2) {
        printf("please run this command 'server <TCP port to listen on>'\n");
        exit(1);
    }
    
    // read login information
    client_head = read_login();
    if (client_head == NULL) {
        printf("no client login information is found\n");
        exit(1);
    }
    
    int sockfd; // listen on sock_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr; // connector's address information socklen_t;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket\n");
            continue;
        }
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt\n");
            exit(1);
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind\n");
            continue;
        }
        
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if(p==NULL){
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen\n");
        exit(1);
    }
    
    printf("Server: Listening for connection on port %s\n", argv[1]);
    
    
    int fdmax = sockfd;
    fd_set read_fds;
    fd_set master;
    FD_ZERO(&read_fds);
    FD_ZERO(&master);
    FD_SET(sockfd, &master);

    while (1) {
        read_fds = master;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            printf("Select error\n");
            exit(1);
        }

        // read_fds will only be left with the fd's that can be read right now
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {

                if (i == sockfd) {
                    // This is the socket that listens for incoming connections.
                    // So establish a new connection here.
                    socklen_t addrlen = sizeof(struct sockaddr_storage);
                    int new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
                    if (new_fd == -1) {
                        printf("Accept connection error\n");
                        continue;
                    }
                    // add the new fd to the file set
                    FD_SET(new_fd, &master);
                    if (new_fd > fdmax) { // keep track of the max
                        fdmax = new_fd;
                    }
                    inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
                    printf("server: new connection from %s\n", s);
                }
                else{
                    // handle communication with clients which have already connected
                    char buf[MAX_STR_LEN];
                    int nbytes = recv(i, buf, MAX_STR_LEN, 0);
                    if(nbytes==-1){
                        printf("Error occurs when server reads from socket. %d\n", errno);
                        exit(1);
                    }
                    if(nbytes == 0 || (nbytes == -1 && errno == 54)){
                        // Client disconnected
                        struct CLIENT_NODE* curr = client_head;
                        while (curr != NULL) {
                            if (curr->sockfd == i) {
                                struct SESSION_NODE* Session = curr->active_session;
                                if (Session) {
                                    remove_client_from_session(Session, curr);
                                }
                                curr->active_session = NULL;
                                curr->sockfd = -1;
                                break;
                            }
                            curr = curr->next;
                        }
                        assert(curr != NULL);
                        printf("Client %s disconnected\n", curr->username);
                        close(i);
                        FD_CLR(i, &master);
                        continue;
                    }
                    buf[nbytes] = '\0';
                    struct message* msg = malloc(sizeof(struct message));
                    deserialize_message(buf, msg);
                    int if_login;
                    switch (msg->type) {
                        case LOGIN:
                            if_login = handle_login(msg, i);
                            if (if_login == -1) {
                                FD_CLR(i, &master);
                            }
                            break;
                        case EXIT:
                            handle_exit(msg);
                            FD_CLR(i, &master);
                            break;
                        case JOIN:
                            join_session(msg, i);
                            break;
                        case LEAVE_SESS:
                            leave_session(msg, i);
                            break;
                        case NEW_SESS:
                            new_session(msg, i);
                            break;
                        case MESSAGE:
                            send_message(msg, i);
                            break;
                        case QUERY:
                            handle_query(msg, i);
                            break;
                        default:
                            printf("No type is matched");
                            exit(1);
                    }
                    free(msg);
                }
            }
            
        }
    }
    
    return (EXIT_SUCCESS);
}


struct CLIENT_NODE* read_login() {
    // Reads the login information from a text file, login.txt
    FILE* fp = fopen(LOGIN_FILE, "r");
    if (fp == NULL) {
        printf("Can't read the login file\n");
        exit(1);
    }

    size_t len = 0;
    char* Line;
    char delim[] = " \t\r\n\v\f";

    struct CLIENT_NODE* head = NULL; 
    struct CLIENT_NODE* curr;

    while (getline(&Line, &len, fp) != -1) {
        if (!head) {
            head = malloc(sizeof(struct CLIENT_NODE));
            curr = head;
        } else {
            curr->next = malloc(sizeof(struct CLIENT_NODE));
            curr = curr -> next;
        }
        //read until whitespace
        char* ptr = strtok(Line, delim);
        strcpy(curr->username, ptr);

        ptr = strtok(NULL, delim);
        strcpy(curr->password, ptr);

        //initialize client_node
        curr->next = NULL;
        curr->active_session = NULL;
        curr->sockfd = -1;
    }
    free(Line);
    fclose(fp);
    return head;
}

struct CLIENT_NODE* get_client_info (const char* username) {
    struct CLIENT_NODE* curr = client_head;
    while (curr != NULL) {
        if (strcmp(username, curr->username) == 0) {
            return curr;
        }
        curr = curr -> next;
    }
    return NULL;
}



struct SESSION_NODE* get_session_info (const char* session_id) {
    struct SESSION_NODE* curr = session_head;
    while (curr != NULL) {
        if (strcmp(session_id, curr->session_id) == 0) {
            return curr;
        }
        curr = curr -> next;
    }
    return NULL;
}

void send_string_to_client(const char* msg_str,int sockfd) {
    // Send string to client
    if (send(sockfd, msg_str, strlen(msg_str), 0) == -1) {
        printf("Error sending message: %d\n", errno);
        exit(1);
    }
}

void send_message_to_client(struct message* msg,int sockfd) {
    // Send TCP message to client
    char* msgStr = malloc(msg->size+128);
    serialize_message(*msg,msgStr);
    printf("Sending message: %s\n", msgStr);
    send_string_to_client(msgStr, sockfd);
    free(msgStr);
}

// Help with deleting a user from a session, and delete the session too if it's now empty
void remove_client_from_session(struct SESSION_NODE* session, struct CLIENT_NODE* client) {
    for (int i = 0; i < SESSION_CAPACITY; i++) {
        if (session->clients[i] == client) {
            session->clients[i] = NULL;
            session->num_client--;

            if (session->num_client == 0) {
                struct SESSION_NODE* prev = session->prev;
                struct SESSION_NODE* next = session->next;
                
                printf("The session %s is deleted\n",session->session_id);
                // No client in the session, delete it
                if (prev && next) {
                    prev->next = next;
                    next->prev = prev;
                } else if (prev) {
                    prev->next = NULL;
                } else {
                    if (next){
                        next->prev = NULL;
                    } 
                    session_head = next;
                }
            } else {
                printf("There are still %d users in session %s\n", session->num_client, session->session_id);
            }
            break;
        }
    }
}

int handle_login(struct message* msg, int sockfd) {
    // Must check the username and password against the known database.
    // If login is successful, a positive fd will be set in matching_username->sockfd.
    // This also sends a response to the client.
    struct message newMsg;
    strcpy(newMsg.source, "server");
    newMsg.type = LO_NAK;

    struct CLIENT_NODE* Username = get_client_info(msg->source);
    //check if username exists
    if (Username) {
        //check if password is valid 
        if (strcmp(msg->data, Username->password) == 0) {
            if (Username->sockfd != -1) {
                strcpy(newMsg.data, "You have logged in somewhere\n");
            } else {
                // successful log in
                Username->sockfd = sockfd;
                newMsg.type = LO_ACK;
                strcpy(newMsg.data, "successfully log in ");
                printf("success\n");
            }
        } else {
            strcpy(newMsg.data, "invalid password");
        }
    } else {
        strcpy(newMsg.data, "username not found");
    }

    newMsg.size = strlen(newMsg.data) + 1;
   
    send_message_to_client(&newMsg,sockfd);
    
    return (newMsg.type == LO_ACK ? 0 : -1);
}

void handle_exit(struct message* msg) {
    struct CLIENT_NODE* Username = get_client_info(msg->source);
    if (Username) {

        // if currently in a session, leave this session
        if (Username->active_session != NULL) {
            struct SESSION_NODE* session = Username->active_session;
            remove_client_from_session(session, Username);
            Username->active_session = NULL;
        }

        close(Username->sockfd);
        Username->sockfd = -1;
    }
}

void join_session(struct message* msg, int sockfd) {
    struct CLIENT_NODE* Username = get_client_info(msg->source);
    struct message newMsg;
    strcpy(newMsg.source, "server");
    newMsg.type = JN_NAK;
    char errorMsg[MAX_STR_LEN];
    
    if(Username){
        if(Username->sockfd == sockfd && Username->active_session == NULL){
            struct SESSION_NODE* Session = get_session_info(msg->data);

            if (Session) {
                int if_joined = 0;
                for (int i = 0; i < SESSION_CAPACITY; i++) {
                    if (Session->clients[i] == NULL) {
                        Session->clients[i] = Username;
                        if_joined = 1;
                        Session->num_client++;
                        Username->active_session = Session;
                        printf("%s joined %s\n", Username->username, msg->data);
                        break;
                    }
                }

                if (!if_joined) {
                    // the session is full
                    sprintf(errorMsg, "%s - the session is full!", msg->data);
                    strcpy(newMsg.data, errorMsg);
                } else { //join session successfully
                    newMsg.type = JN_ACK;
                    strcpy(newMsg.data, msg->data);
                }
            } else {
                sprintf(errorMsg, "%s - you entered an invalid session ID", msg->data);
                strcpy(newMsg.data, errorMsg);
            }
        } else if(Username->sockfd != sockfd){
            // client hasn't logged in yet
            sprintf(errorMsg, "%s - you need to log in", msg->source);
            strcpy(newMsg.data, errorMsg);
        }else if(Username->active_session != NULL){
            //the client is already in one session
            sprintf(errorMsg, "%s - you're already in a session. Leave the session.", msg->source);
            strcpy(newMsg.data, errorMsg);
        }
    }else{
        //The client is not recognized 
        sprintf(errorMsg, "%s - client ID unrecognized.", msg->source);
        strcpy(newMsg.data, errorMsg);
    }
    newMsg.size = strlen(newMsg.data) + 1;
    send_message_to_client(&newMsg,sockfd);
}

void leave_session(struct message* msg, int sockfd) {
    struct CLIENT_NODE* Username = get_client_info(msg->source);
    // leave the current session
    if(Username) {
        if(Username->sockfd == sockfd && Username->active_session != NULL){
            struct SESSION_NODE *Session = Username->active_session;
            remove_client_from_session(Session, Username);
            Username->active_session = NULL;
        }
    }
}

void new_session(struct message* msg, int sockfd) {
    struct CLIENT_NODE* Username = get_client_info(msg->source);
    struct message newMsg;
    strcpy(newMsg.source, "server");
    char errorMsg[MAX_STR_LEN];
    if(Username){
        if(Username->sockfd == sockfd && Username->active_session == NULL){
            //check if a session already exists with this session ID
            struct SESSION_NODE* Session = get_session_info(msg->data);
            if(Session == NULL){
                printf("create successfully\n");
                //add session node to head  of linked list
                struct SESSION_NODE *newSession = malloc(sizeof(struct SESSION_NODE));
                if (session_head) {
                    session_head->prev = newSession;
                }
                newSession->next = session_head;
                newSession->prev = NULL;
                session_head = newSession;
                strcpy(newSession->session_id, msg->data);
                newSession->num_client = 1;
                for (int client = 0; client < SESSION_CAPACITY; client++) {
                    if (client == 0) {
                        newSession->clients[client] = Username;
                        Username->active_session = newSession;
                    } else {
                        newSession->clients[client] = NULL;
                    }
                }
                newMsg.type = NS_ACK;
                strcpy(newMsg.data, msg->data);
            }else{
                // a session already exists with this name
                sprintf(errorMsg, "%s - a session already exists with this name", msg->data);
                strcpy(newMsg.data, errorMsg);
            }
        }else if(Username->active_session != NULL){
            //client in another session already
            sprintf(errorMsg, "%s - you need to exit the current session", msg->source);
            strcpy(newMsg.data, errorMsg);
        }else{
            //client hasn't logged in
            sprintf(errorMsg, "%s - you need to log in", msg->source);
            strcpy(newMsg.data, errorMsg);
        }
    }else{
        //the client is not recognized
        sprintf(errorMsg, "%s - client ID unrecognized", msg->source);
        strcpy(newMsg.data, errorMsg);
    }
    newMsg.size = strlen(newMsg.data) + 1;
    send_message_to_client(&newMsg,sockfd);
}

void send_message(struct message* msg, int sockfd) {
    struct CLIENT_NODE* Username = get_client_info(msg->source);
    if (Username){
        if(Username->sockfd == sockfd){
            struct SESSION_NODE* Session = Username->active_session;
            if(Session){
                char* Str = malloc(msg->size + 128);
                serialize_message(*msg,Str);
                for (int i = 0; i < SESSION_CAPACITY; i++) {
                    if(Session->clients[i] != NULL && Session->clients[i] != Username){
                        send_string_to_client(Str,Session->clients[i]->sockfd);
                    }
                }
                free(Str);
            }
        }
    }
} 

void handle_query(struct message* msg, int sockfd) {
    // Sends the list of users, and their active sessions as reply message.

    struct message newMsg;
    strcpy(newMsg.source, "server");
    newMsg.type = QU_ACK;
    newMsg.data[0] = '\0';
    
    struct CLIENT_NODE* curr = client_head;
    int curr_length = 0;
    
    while (curr != NULL) {
        if (curr->sockfd != -1) {
            curr_length += strlen(curr->username) + 3;
            strcat(newMsg.data, curr->username);
            strcat(newMsg.data, " - ");
            if (curr->active_session == NULL) {
                curr_length += 11;
                strcat(newMsg.data, "No session\n");
            } else {
                curr_length += (strlen(curr->active_session->session_id) + 1);
                strcat(newMsg.data, curr->active_session->session_id);
                newMsg.data[curr_length - 1] = '\n';
                newMsg.data[curr_length] = '\0'; // This let next strcat to concatenate rightly
            }
        }
        if (MAX_DATA - MAX_NAME - MAX_SESSION_ID - 10 < curr_length) {
            // can't fit, so can't put any more data
            curr_length += 3;
            strcat(newMsg.data, "..\n");
            break;
        }
        curr = curr->next;
    }
    newMsg.data[curr_length] = '\0';
    newMsg.size = curr_length +1;
    send_message_to_client(&newMsg,sockfd);
}

