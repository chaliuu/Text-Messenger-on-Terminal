/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 * Click nbfs://nbhost/SystemFileSystem/Templates/cFiles/file.h to edit this template
 */

/* 
 * File:   server.h
 * Author: wengchan
 *
 * Created on March 10, 2024, 4:04 PM
 */

#ifndef SERVER_H
#define SERVER_H

#include "packet.h"

struct CLIENT_NODE;
struct SESSION_NODE;

struct CLIENT_NODE {
    int sockfd;
    char username [MAX_NAME];
    char password [MAX_PASSWD];
    struct SESSION_NODE* active_session;
    struct CLIENT_NODE* next;
};

struct SESSION_NODE {
    int num_client;
    char session_id[MAX_SESSION_ID];
    struct CLIENT_NODE* clients [SESSION_CAPACITY];
    struct SESSION_NODE* prev;
    struct SESSION_NODE* next;
};

struct CLIENT_NODE* read_login();

void remove_user_from_session(struct SESSION_NODE* session, struct CLIENT_NODE* client);

struct SESSION_NODE* get_session_info (const char* session_id);

struct SESSION_NODE* create_new_session (struct SESSION_NODE* head, char* session_id, struct CLIENT_NODE* client);

struct SESSION_NODE* remove_from_session(struct CLIENT_NODE* client);

void send_message_to_client(int sockfd, struct message* msg);

void send_string_to_client(int sockfd, const char* msg_str);

int handle_login (struct message* msg, int sockfd);

void handle_exit(struct message* msg);

void handle_join_session(struct message* msg, int sockfd);

void handle_leave_session(struct message* msg, int sockfd);

void handle_new_session(struct message* msg, int sockfd);

void handle_send_message(struct message* msg, int sockfd);

void handle_query(struct message* msg, int sockfd);




#endif /* SERVER_H */

