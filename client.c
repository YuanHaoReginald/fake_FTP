//
// Created by reginald on 10/29/17.
//

#include "tools.h"

void get_ip_and_port(int* target_port, char* target_ip);
int get_response(int connfd, char* command, char *buffer, size_t buffer_size);
int check_response(char* response, char* response_expect);
void input_with_hint(char* hint, char* buffer, int buffer_size, char* default_buffer);
int login(int connfd);
int set_type_and_mode(int connfd, int* mode);
int trander_file_pasv(int connfd, char* filename, int flag_retr_or_stor);
int trander_file_port(int connfd, char* filename, int flag_retr_or_stor);
int tranfer_file_client(int connfd, int transfer_fd, int flag_retr_or_stor, char* filename);


int main() {
    int connectfd;
    char response_buffer[8192];
    char target_ip[20];
    int target_port;
    int mode = 1;

    get_ip_and_port(&target_port, target_ip);
    printf("Connecting to %s:%d\n", target_ip, target_port);

    if(socket_connect_client(&connectfd, target_port, target_ip) != 0) {
        close(connectfd);
        return 1;
    }
    printf("Connected\n");

    if(get_response(connectfd, "", response_buffer, 8191) == -1) {
        close(connectfd);
        return 1;
    }
    if(check_response(response_buffer, "220") == 0) {
        close(connectfd);
        return 1;
    }

    if(login(connectfd) == 1){
        close(connectfd);
        return 1;
    }
    if(set_type_and_mode(connectfd, &mode) == 1){
        close(connectfd);
        return 1;
    }

    while(1) {
        char command_buffer[8192];
        char command[8192];
        char parameter[8192];
        input_with_hint("Command", command_buffer, 8191, "");
        parse_command(command_buffer, command, parameter);
        printf("command '%s', parameter '%s'\n", command, parameter);

        char request[8192];
        if(strcmp(command, "exit") == 0) {
            if(get_response(connectfd, "QUIT\r\n", response_buffer, 8191) == -1) {
                close(connectfd);
                return 1;
            }
            if(check_response(response_buffer, "221") == 0) {
                printf("%s\n", response_buffer);
                return 1;
            }
            break;
        } else if (strcmp(command, "help") == 0) {
            printf("Here is the command you can write:\n");
            printf("\thelp                    \t--show this info.\n");
            printf("\tdownload <parameter>    \t--download file.\n");
            printf("\tupload <parameter>      \t--upload file.\n");
            printf("\tcd <parameter>          \t--cd a floder.\n");
            printf("\tls <parameter>[optional]\t--ls the floder.\n");
            printf("\tmkdir <parameter>       \t--cd a floder.\n");
            printf("\trm <parameter>          \t--cd a floder.\n");
            printf("\texit                    \t--exit the link.\n");
            continue;
        } else if (strcmp(command, "download") == 0) {
            if(access(parameter, 0) != -1) {
                printf("The file is exist!\n");
                continue;
            }
            if(mode == 2) {
                trander_file_port(connectfd, parameter, 0);
            } else {
                trander_file_pasv(connectfd, parameter, 0);
            }
        } else if (strcmp(command, "upload") == 0) {
            if(access(parameter, 0) == -1) {
                printf("The file not exist!\n");
                continue;
            }
            if(access(parameter, 4) == -1) {
                printf("The file can not read!\n");
                continue;
            }
            if(mode == 2) {
                trander_file_port(connectfd, parameter, 1);
            } else {
                trander_file_pasv(connectfd, parameter, 1);
            }
        } else if (strcmp(command, "cd") == 0) {
            sprintf(request, "CWD %s\r\n", parameter);
            if(get_response(connectfd, request, response_buffer, 8191) == -1) {
                close(connectfd);
                return 1;
            }
            if(check_response(response_buffer, "250") == 0) {
                printf("%s\n", response_buffer);
            }
            continue;
        } else if (strcmp(command, "ls") == 0) {

        } else if (strcmp(command, "mkdir") == 0) {
            sprintf(request, "MKD %s\r\n", parameter);
            if(get_response(connectfd, request, response_buffer, 8191) == -1) {
                close(connectfd);
                return 1;
            }
            if(check_response(response_buffer, "250") == 0) {
                printf("%s\n", response_buffer);
            }
            continue;
        } else if (strcmp(command, "rm") == 0) {
            sprintf(request, "RMD %s\r\n", parameter);
            if(get_response(connectfd, request, response_buffer, 8191) == -1) {
                close(connectfd);
                return 1;
            }
            if(check_response(response_buffer, "250") == 0) {
                printf("%s\n", response_buffer);
            }
            continue;
        } else {
            printf("This command not support!\n");
        }
    }

    close(connectfd);
    printf("Bye!\n");
    return 0;
}

void get_ip_and_port(int* target_port, char* target_ip) {
    char server[32];
    input_with_hint("Server IP and Port, default: 127.0.0.1:21",
                    server, 31, "127.0.0.1:21");
    parse_ip_and_port(server, target_port, target_ip, 0);
}

int get_response(int connfd, char* command, char *buffer, size_t buffer_size) {
    if(strlen(command))
        if(send_command(connfd, command) == -1)
            return -1;
    if(recv_command(connfd, buffer, buffer_size) == -1)
        return -1;
    remove_space(buffer);
    return 0;
}

int check_response(char* response, char* response_expect) {
    if(start_with(response, response_expect) == 1) {
        return 1;
    }
    return 0;
}

void input_with_hint(char* hint, char* buffer, int buffer_size, char* default_buffer) {
    printf("\n%s > ", hint);
    fgets(buffer, buffer_size, stdin);
    remove_space(buffer);
    if(strlen(buffer) == 0){
        strcpy(buffer, default_buffer);
    }
}

int login(int connfd){
    char username[8192];
    char password[8192];
    while (1) {
        while(1) {
            input_with_hint("Username, default: anonymous", username, 8191, "anonymous");
            char command_username[8192];
            sprintf(command_username, "USER %s\r\n", username);
            char response_buffer[8192];
            if(get_response(connfd, command_username, response_buffer, 8191) == -1) {
                return 1;
            }
            printf("%s\n", response_buffer);

            if(check_response(response_buffer, "331")){
                break;
            }
        }

        while(1) {
            input_with_hint("Password, please use your e-mail",
                            password, 8191, "");
            char command_password[8192];
            sprintf(command_password, "PASS %s\r\n", password);
            char response_buffer[8192];
            if(get_response(connfd, command_password, response_buffer, 8191) == -1) {
                return 1;
            }
            printf("%s\n", response_buffer);
            if(check_response(response_buffer, "501")) {
                goto login_continue;
            } else if (check_response(response_buffer, "230")) {
                return 0;
            }
        }
        login_continue:
        ;
    }
}

int set_type_and_mode(int connfd, int* mode) {
    //mode: 1:PASV, 2:PORT
    char command_type[8192];
    sprintf(command_type, "TYPE I\r\n");
    char response_buffer[8192];
    if(get_response(connfd, command_type, response_buffer, 8191) == -1) {
        return 1;
    }
    if(check_response(response_buffer, "200") == 0){
        return 1;
    }

    while(1) {
        char mode_buffer[8192];
        input_with_hint("PORT or PASV mode, default: PASV",
                        mode_buffer, 8191, "PASV");
        if(start_with(mode_buffer, "PASV")){
            *mode = 1;
            break;
        } else if(start_with(mode_buffer, "PORT")) {
            *mode = 2;
            break;
        } else {
            printf("Please input true mode!\n");
        }
    }
    return 0;
}

int trander_file_pasv(int connfd, char* filename, int flag_retr_or_stor){ //flag_retr_or_stor: 0: retr, 1: stor
    char response_buffer[8192];
    char command_buffer[8192];
    int transfer_fd;
    if(get_response(connfd, "PASV\r\n", response_buffer, 8191) == -1) {
        return 1;
    }
    if(check_response(response_buffer, "227") == 0){
        printf("%s\n", response_buffer);
        return 1;
    }

    char* temp_ptr = strchr(response_buffer, ' ');
    if(temp_ptr == NULL)
        return 1;
    while(*temp_ptr < '0' || *temp_ptr > '9') {
        temp_ptr++;
    }
    char ip_and_port [8192];
    strcpy(ip_and_port, temp_ptr);
    while(ip_and_port[strlen(ip_and_port) - 1] < '0' || ip_and_port[strlen(ip_and_port) - 1] > '9') {
        ip_and_port[strlen(ip_and_port) - 1] = '\0';
    }
    char ip[8192];
    int port;
    parse_ip_and_port(ip_and_port, &port, ip, 1);
    sleep(1);

    sprintf(command_buffer, (flag_retr_or_stor == 0? "RETR %s\r\n": "STOR %s\r\n"), filename);
    if(get_response(connfd, command_buffer, response_buffer, 8191) == -1) {
        return 1;
    }
    if(check_response(response_buffer, "150") == 0){
        printf("%s\n", response_buffer);
        return 1;
    }
    if(socket_connect_client(&transfer_fd, port, ip) != 0) {
        return 1;
    }
    if(tranfer_file_client(connfd, transfer_fd, flag_retr_or_stor, filename) != 0) {
        close(transfer_fd);
        return 1;
    }
    close(transfer_fd);
    return 0;
}

int trander_file_port(int connfd, char* filename, int flag_retr_or_stor){ //flag_retr_or_stor: 0: retr, 1: stor
    char response_buffer[8192];
    char command_buffer[8192];
    int transfer_fd, pasv_fd;
    int r = socket_bind_listen(&pasv_fd, 0);
    if(r != 0) {
        printf("Some error happen when bind or listen.\n");
        return 1;
    }

    struct sockaddr_in local_addr;
    socklen_t len;
    bzero(&local_addr, sizeof(local_addr));
    getsockname(connfd, (struct sockaddr*)&local_addr, &len);
    int ip_int_list[4];
    parse_ip(inet_ntoa(local_addr.sin_addr), ip_int_list);
    getsockname(pasv_fd, (struct sockaddr*)&local_addr, &len);
    int port = ntohs(local_addr.sin_port);

    sprintf(command_buffer, "PORT %d,%d,%d,%d,%d,%d\r\n",
            ip_int_list[0], ip_int_list[1], ip_int_list[2], ip_int_list[3], port / 256, port % 256);
    if(get_response(connfd, command_buffer, response_buffer, 8191) == -1) {
        close(pasv_fd);
        return 1;
    }
    if(check_response(response_buffer, "200") == 0){
        printf("%s\n", response_buffer);
        close(pasv_fd);
        return 1;
    }
    sleep(1);

    sprintf(command_buffer, (flag_retr_or_stor == 0? "RETR %s\r\n": "STOR %s\r\n"), filename);
    if(get_response(connfd, command_buffer, response_buffer, 8191) == -1) {
        return 1;
    }
    if(check_response(response_buffer, "150") == 0){
        printf("%s\n", response_buffer);
        return 1;
    }
    if((transfer_fd = accept(pasv_fd, NULL, NULL)) == -1) {
        printf("Error accept(): %s(%d)\n", strerror(errno), errno);
        send_command(connfd, "425 Can not set a socket.\r\n");
        close(pasv_fd);
        return 1;
    }
    if(tranfer_file_client(connfd, transfer_fd, flag_retr_or_stor, filename) != 0) {
        close(transfer_fd);
        close(pasv_fd);
        return 1;
    }
    close(transfer_fd);
    close(pasv_fd);
    return 0;
}

int tranfer_file_client(int connfd, int transfer_fd, int flag_retr_or_stor, char* filename){
    char response_buffer[8192];
    if((flag_retr_or_stor == 0 ? recv_file: send_file)(transfer_fd, filename) != 0) {
        return 1;
    }
    close(transfer_fd);
    if(get_response(connfd, "", response_buffer, 8191) == -1) {
        return 1;
    }
    if(check_response(response_buffer, "226") == 0){
        printf("%s\n", response_buffer);
        return 1;
    }
    printf("Successfully %sloaded `%s`.\n", (flag_retr_or_stor == 0? "down": "up"), filename);
    return 0;
}
