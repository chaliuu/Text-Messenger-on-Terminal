/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 * Click nbfs://nbhost/SystemFileSystem/Templates/cFiles/file.h to edit this template
 */

/* 
 * File:   server.h
 * Author: wengchan
 *
 * Created on March 23, 2024, 5:06 PM
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

struct CLIENT_NODE* get_client_info (const char* username);

struct SESSION_NODE* get_session_info (const char* session_id);

void send_string_to_client(const char* msg_str,int sockfd);

void send_message_to_client(struct message* msg,int sockfd);

void remove_client_from_session(struct SESSION_NODE* session, struct CLIENT_NODE* client);

int handle_login (struct message* msg, int sockfd);

void handle_exit(struct message* msg);

void join_session(struct message* msg, int sockfd);

void leave_session(struct message* msg, int sockfd);

void new_session(struct message* msg, int sockfd);

void send_message(struct message* msg, int sockfd);

void handle_query(struct message* msg, int sockfd);

void private_message(struct message* msg, int sockfd);

void register_user(struct message* msg, int sockfd);


#endif /* SERVER_H */

