/*
 * Click nbfs://nbhost/SystemFileSystem/Templates/Licenses/license-default.txt to change this license
 * Click nbfs://nbhost/SystemFileSystem/Templates/cFiles/file.h to edit this template
 */

/* 
 * File:   packet.h
 * Author: wengchan
 *
 * Created on March 23, 2024, 5:05 PM
 */

#ifndef PACKET_H
#define PACKET_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MAX_NAME 32
#define MAX_PASSWD 20
#define MAX_DATA 1024
#define MAX_STR_LEN MAX_NAME+MAX_PASSWD+MAX_DATA
#define MAX_SESSION_ID 20
#define SESSION_CAPACITY 20

enum msg_type {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK,
    PRIVATE_MESSAGE,//private message to another client
    PM_ACK,
    PM_NAK,
    REGISTER,//user registration
    REG_ACK,
    REG_NAK,
};

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

// Serializes a message structure into a string
void serialize_message(struct message msg, char* output){
    sprintf(output, "%u:%u:%s:%s",msg.type, msg.size, msg.source, msg.data);
}

// Deserializes a string into a message structure
void deserialize_message(char *input, struct message *msg) {
    sscanf(input, "%u:%u:%[^:]:%[^:]", &msg->type, &msg->size, msg->source, msg->data);
}


#endif /* PACKET_H */

