//
// Created by reginald on 10/29/17.
//

#ifndef FAKE_FTP_TOOLS_H
#define FAKE_FTP_TOOLS_H

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>

extern void parse_ip_and_port(char* ip_and_port, int* target_port, char* target_ip, int type);
extern int start_with(const char* sentence, char* word);
extern int send_command(int connfd, char* str);
extern int recv_command(int connfd, char* buffer, size_t len);
extern void remove_space(char* buffer);
extern void parse_command(char* buffer, char* command, char* parameter);
extern void parse_ip(char* ip, int* ip_int_list);
extern int socket_bind_listen(int *listenfd, int listening_port);
extern int socket_connect_client(int* connectfd, int target_port, char* target_ip);
extern int send_file(int connfd, char* filename);
extern int recv_file(int connfd, char* filename);

#endif //FAKE_FTP_TOOLS_H
