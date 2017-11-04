//
// Created by reginald on 10/29/17.
//


#include "tools.h"

char ** user_list = NULL;
int user_number = 0;

void parse_input(int argc, char **argv, int* listening_port, char* working_directory);
int get_user_list();
void accept_loop(int listenfd);
void handle_client(int connfd);
int password_is_right(const char* username, const char* password);
int parse_index(char* index, int* current_height, int can_change); //0:not 1:can

int main(int argc, char **argv) {
    int listening_port = 21;
    char working_directory[8192] = "/tmp";
    parse_input(argc, argv, &listening_port, working_directory);
    printf("work in %s, listen on %d\n", working_directory, listening_port);
    user_number = get_user_list();
    if(user_number == -1) {
        return 1;
    }
    chdir(working_directory);

    int listenfd;
    if(socket_bind_listen(&listenfd, listening_port) != 0){
        return 1;
    }
    accept_loop(listenfd);
    for(int i = 0; i < user_number; ++i) {
        free(user_list[i]);
    }
    free(user_list);
    close(listenfd);
    return 0;
}

void parse_input(int argc, char **argv, int* listening_port, char* working_directory) {
    struct option long_options[] = {
            {"root", required_argument, NULL, 'r'},
            {"port", required_argument, NULL, 'p'},
            {NULL, 0, NULL, 0}
    };
    while (1){
        switch(getopt_long_only(argc, argv, "r:p:", long_options, NULL)){
            case 'r':
                strcpy(working_directory, optarg);
                break;
            case 'p':
                *listening_port = atoi(optarg);
                break;
            default:
                return;
        }
    }
}

int get_user_list() {
    int len = 0;
    FILE *fp = fopen("user_list.txt", "r");
    if(NULL == fp) {
        printf("failed to open user_list.txt\n");
        return -1;
    }
    while(!feof(fp)) {
        char* temp = (char*)malloc(21 * sizeof(char));
        fgets(temp, 20, fp);
        free(temp);
        len++;
    }
    fclose(fp);
    len--;

    user_list = (char**)malloc(len * sizeof(char*));
    fp = fopen("user_list.txt", "r");
    if(NULL == fp) {
        printf("failed to open user_list.txt\n");
        return -1;
    }
    int i = 0;
    while(!feof(fp)) {
        user_list[i] = (char*)malloc(21 * sizeof(char));
        fgets(user_list[i], 20, fp);
        remove_space(user_list[i]);
        i++;
    }
    fclose(fp);
    return len;
}

void accept_loop(int listenfd) {
    int connfd;
    while(1) {
        if ((connfd = accept(listenfd, NULL, NULL)) == -1) {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
            continue;
        }else{
            int pid = fork();
            if(pid < 0){
                printf("Error fork()\n");
            }else if(pid == 0){
                handle_client(connfd);
                close(connfd);
                return;
            }
        }
        close(connfd);
    }
}

void handle_client(int connfd) {
    send_command(connfd, "220 Anonymous FTP server ready.\r\n");
    int flag_has_username = 0;
    int flag_has_log = 0;
    char username[8192];
    int flag_pasv_or_port_mode = 0; //pasv:1 port:2
    int pasv_fd = 0;
    char port_parameter[8192];
    int height = 0;


    while(1) {
        char buffer[8192];
        char command[8192];
        char parameter[8192];

        int command_length = recv_command(connfd, buffer, 8191);
        if (command_length < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            if(pasv_fd != 0){
                close(pasv_fd);
                pasv_fd = 0;
            }

            return;
        } else if (command_length == 0) {
            printf("User log out!\n");
            if(pasv_fd != 0){
                close(pasv_fd);
                pasv_fd = 0;
            }
            return;
        }
        printf("Heard %d bytes.\n", command_length);
        parse_command(buffer, command, parameter);
        printf("command '%s', parameter '%s'\n", command, parameter);

        if(strcmp(command, "USER") == 0) {
            if(flag_has_username == 1){
                send_command(connfd, "530 You have input username already!\r\n");
                goto outer_continue;
            }
            for(int i = 0; i < user_number; ++i){
                if(strlen(user_list[i]) >= 1 && strcmp(user_list[i], parameter) == 0){
                    flag_has_username = 1;
                    strcpy(username, parameter);
                    send_command(connfd, "331 Username accepted. Need password(please use your email).\r\n");
                    goto outer_continue;
                }
            }
            send_command(connfd, "530 You should use right username!\r\n");
        } else if(strcmp(command, "PASS") == 0) {
            if(flag_has_log == 1) {
                send_command(connfd, "530 You have log in already!\r\n");
                goto outer_continue;
            }
            if(flag_has_username == 0) {
                send_command(connfd, "530 You have not input username before!\r\n");
                goto outer_continue;
            }
            if(strlen(parameter) == 0) {
                send_command(connfd, "530 You should input password!\r\n");
                goto outer_continue;
            }
            if(password_is_right(username, parameter)) {
                flag_has_log = 1;
                send_command(connfd, "230 Access granted.\r\n");
                printf("User '%s' log in!\n", username);
            } else {
                flag_has_username = 0;
                send_command(connfd, "530 Authentication failed.\r\n");
            }
        } else if(strcmp(command, "RETR") == 0) {
            int transfer_fd;
            if(flag_pasv_or_port_mode == 1) {
                if((transfer_fd = accept(pasv_fd, NULL, NULL)) == -1) {
                    printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                    send_command(connfd, "425 Can not set a socket.\r\n");
                    goto retr_outer_continue;
                }
                send_command(connfd, "150 Opening BINARY mode data connection.\r\n");
            } else if (flag_pasv_or_port_mode == 2) {
                char client_ip[8192];
                int client_port;
                parse_ip_and_port(port_parameter, &client_port, client_ip, 1);
                if(socket_connect_client(&transfer_fd, client_port, client_ip) != 0) {
                    printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                    send_command(connfd, "425 Can not set a socket.\r\n");
                    goto retr_outer_continue;
                }
                send_command(connfd, "150 Opening BINARY mode data connection.\r\n");
            } else {
                send_command(connfd, "425 PASV or PORT may not be set before.\r\n");
                goto retr_outer_continue;
            }
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                close(transfer_fd);
                goto retr_outer_continue;
            }
            if(parse_index(parameter, &height, 0) == 1) {
                send_command(connfd, "550 You don't have enough right!\r\n");
                close(transfer_fd);
                goto retr_outer_continue;
            }
            if(access(parameter, 0) == -1) {
                send_command(connfd, "550 The file not exist!\r\n");
                close(transfer_fd);
                goto retr_outer_continue;
            }
            if(access(parameter, 4) == -1) {
                send_command(connfd, "550 The file can not read!\r\n");
                close(transfer_fd);
                goto retr_outer_continue;
            }

            int send_file_status = send_file(transfer_fd, parameter);
            if(send_file_status == 1) {
                send_command(connfd, "550 Read file fail!\r\n");
                close(transfer_fd);
                goto retr_outer_continue;
            } else if (send_file_status == 2) {
                send_command(connfd, "426 Send file error!\r\n");
                close(transfer_fd);
                goto retr_outer_continue;
            }
            send_command(connfd, "226 Transfer successful.\r\n");
            close(transfer_fd);

            retr_outer_continue:
            if(pasv_fd != 0){
                close(pasv_fd);
                pasv_fd = 0;
            }
            flag_pasv_or_port_mode = 0;
        } else if(strcmp(command, "STOR") == 0) {
            int transfer_fd;
            if(flag_pasv_or_port_mode == 1) {
                if((transfer_fd = accept(pasv_fd, NULL, NULL)) == -1) {
                    printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                    send_command(connfd, "425 Can not set a socket.\r\n");
                    goto stor_outer_continue;
                }
                send_command(connfd, "150 Opening BINARY mode data connection.\r\n");
            } else if (flag_pasv_or_port_mode == 2) {
                char client_ip[8192];
                int client_port;
                parse_ip_and_port(port_parameter, &client_port, client_ip, 1);
                if(socket_connect_client(&transfer_fd, client_port, client_ip) != 0) {
                    printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                    send_command(connfd, "425 Can not set a socket.\r\n");
                    goto stor_outer_continue;
                }
                send_command(connfd, "150 Opening BINARY mode data connection.\r\n");
            } else {
                send_command(connfd, "425 PASV or PORT may not be set before.\r\n");
                goto stor_outer_continue;
            }
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                close(transfer_fd);
                goto stor_outer_continue;
            }
            if(access(parameter, 0) != -1) {
                send_command(connfd, "550 The file is exist!\r\n");
                close(transfer_fd);
                goto stor_outer_continue;
            }
            int recv_file_status = recv_file(transfer_fd, parameter);
            if(recv_file_status == 1) {
                send_command(connfd, "550 Open file fail!\r\n");
                close(transfer_fd);
                goto stor_outer_continue;
            } else if (recv_file_status == 2) {
                send_command(connfd, "426 Recv file error!\r\n");
                close(transfer_fd);
                goto stor_outer_continue;
            }
            send_command(connfd, "226 Transfer successful.\r\n");
            close(transfer_fd);

            stor_outer_continue:
            if(pasv_fd != 0){
                close(pasv_fd);
                pasv_fd = 0;
            }
            flag_pasv_or_port_mode = 0;
        } else if(strcmp(command, "QUIT") == 0) {
            send_command(connfd, "221 Good bye.\r\n");
            printf("User log out!\n");
            return;
        } else if(strcmp(command, "ABOR") == 0) {
            send_command(connfd, "221 Good bye.\r\n");
            printf("User log out!\n");
            return;
        } else if(strcmp(command, "SYST") == 0) {
            send_command(connfd, "215 UNIX Type: L8\r\n");
        } else if(strcmp(command, "TYPE") == 0) {
            if(strcmp(parameter, "I") == 0){
                send_command(connfd, "200 Type set to I.\r\n");
            } else {
                send_command(connfd, "501 Unexpected parameter.\r\n");
            }
        } else if(strcmp(command, "PORT") == 0) {
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                goto outer_continue;
            }
            if(flag_pasv_or_port_mode == 1) {
                if(pasv_fd != 0){
                    close(pasv_fd);
                    pasv_fd = 0;
                }
            }
            flag_pasv_or_port_mode = 2;
            strcpy(port_parameter, parameter);
            send_command(connfd, "200 PORT accepted\r\n");
        } else if(strcmp(command, "PASV") == 0) {
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                goto outer_continue;
            }
            if(flag_pasv_or_port_mode == 1) {
                if(pasv_fd != 0){
                    close(pasv_fd);
                    pasv_fd = 0;
                }
            }
            int r = socket_bind_listen(&pasv_fd, 0);
            if(r != 0) {
                printf("Some error happen when bind or listen.\n");
                send_command(connfd, "425 Some error happen when bind or listen.\r\n");
                goto outer_continue;
            }
            flag_pasv_or_port_mode = 1;

            struct sockaddr_in local_addr;
            socklen_t len;
            bzero(&local_addr, sizeof(local_addr));
            getsockname(connfd, (struct sockaddr*)&local_addr, &len);
            int ip_int_list[4];
            parse_ip(inet_ntoa(local_addr.sin_addr), ip_int_list);
            getsockname(pasv_fd, (struct sockaddr*)&local_addr, &len);
            int port = ntohs(local_addr.sin_port);
            char response[200];
            sprintf(response, "227 =%d,%d,%d,%d,%d,%d\r\n",
                    ip_int_list[0], ip_int_list[1], ip_int_list[2], ip_int_list[3],
                    port / 256, port % 256);
            send_command(connfd, response);
        } else if(strcmp(command, "MKD") == 0) {
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                goto outer_continue;
            }
            if(parse_index(parameter, &height, 0) == 1) {
                send_command(connfd, "550 You don't have enough right!\r\n");
                goto outer_continue;
            }
            char dir_buffer[8192];
            sprintf(dir_buffer, "mkdir %s", parameter);
            if(system(dir_buffer) != 0) {
                send_command(connfd, "550 MKD fail!\r\n");
            } else {
                send_command(connfd, "250 MKD success!\r\n");
            }
        } else if(strcmp(command, "CWD") == 0) {
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                goto outer_continue;
            }
            if(parse_index(parameter, &height, 0) == 1) {
                send_command(connfd, "550 You don't have enough right!\r\n");
                goto outer_continue;
            }
            if(chdir(parameter) == -1) {
                send_command(connfd, "550 CWD fail!\r\n");
            } else {
                parse_index(parameter, &height, 1);
                send_command(connfd, "250 CWD success!\r\n");
            }
        } else if(strcmp(command, "LIST") == 0) {
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                goto list_outer_continue;
            }
            if(parse_index(parameter, &height, 0) == 1) {
                send_command(connfd, "550 You don't have enough right!\r\n");
                goto list_outer_continue;
            }
            int transfer_fd;
            if(flag_pasv_or_port_mode == 1) {
                send_command(connfd, "150 Opening BINARY mode data connection.\r\n");

                if((transfer_fd = accept(pasv_fd, NULL, NULL)) == -1) {
                    printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                    send_command(connfd, "425 Can not set a socket.\r\n");
                    goto list_outer_continue;
                }
            } else if (flag_pasv_or_port_mode == 2) {
                send_command(connfd, "150 Opening BINARY mode data connection.\r\n");

                char client_ip[8192];
                int client_port;
                parse_ip_and_port(port_parameter, &client_port, client_ip, 1);
                if(socket_connect_client(&transfer_fd, client_port, client_ip) != 0) {
                    printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                    send_command(connfd, "425 Can not set a socket.\r\n");
                    goto list_outer_continue;
                }
            } else {
                send_command(connfd, "425 PASV or PORT may not be set before.\r\n");
                goto list_outer_continue;
            }

            char dir_buffer[8192];
            sprintf(dir_buffer, "ls -l %s", parameter);
            FILE *fp;
            if ((fp = popen(dir_buffer, "r")) == NULL) {
                send_command(connfd, "550 LIST fail!\r\n");
                goto list_outer_continue;
            }
            char list_buffer[8192];
            size_t n;
            while (1){
                n = fread(list_buffer, 1, 8190, fp);
                printf("%s", list_buffer);
                if(send(transfer_fd, list_buffer, n, 0) < 0) {
                    printf("Error send() in LIST\n");
                    send_command(connfd, "426 LIST send error!\r\n");
                    pclose(fp);
                    goto list_outer_continue;
                }
                if(n <= 0)
                    break;
            }
            pclose(fp);
            send_command(connfd, "226 LIST success!\r\n");
            close(transfer_fd);

            list_outer_continue:
            if(pasv_fd != 0){
                close(pasv_fd);
                pasv_fd = 0;
            }
            flag_pasv_or_port_mode = 0;
        } else if(strcmp(command, "RMD") == 0) {
            if(flag_has_log == 0) {
                send_command(connfd, "530 You should log in!\r\n");
                goto outer_continue;
            }
            if(parse_index(parameter, &height, 0) == 1) {
                send_command(connfd, "550 You don't have enough right!\r\n");
                goto outer_continue;
            }
            char dir_buffer[8192];
            sprintf(dir_buffer, "rm -rf %s", parameter);
            if(system(dir_buffer) != 0) {
                send_command(connfd, "550 RMD fail!\r\n");
            } else {
                send_command(connfd, "250 RMD success!\r\n");
            }
        } else {
            send_command(connfd, "500 This Command is not support!\r\n");
        }
        outer_continue:
        ;
    }
}

int password_is_right(const char* username, const char* password){
    (void) username;
    (void) password;
    return 1;
}

int parse_index(char* index, int* current_height, int can_change){
    int height = *current_height;
    if(strlen(index) == 0)
        return 0;
    if(index[0] == '/')
        return 1;
    char buffer[8192];
    strcpy(buffer, index);
    char* point_last = buffer;
    char* point = strchr(buffer, '/');
    while (point != NULL) {
        *point = '\0';
        if(strcmp(point_last, "..") == 0){
            height--;
            if(height < 0)
                return 1;
        } else if (strlen(point_last) != 0 && strcmp(point_last, ".") != 0) {
            height++;
        }
        point_last = point + 1;
        point = strchr(point_last, '/');
    }
    if(strcmp(point_last, "..") == 0){
        height--;
        if(height < 0)
            return 1;
    } else if (strlen(point_last) != 0 && strcmp(point_last, ".") != 0) {
        height++;
    }
    if(can_change == 1)
        *current_height = height;
    return 0;
}