//
// Created by reginald on 17-10-30.
//

#include "tools.h"

void parse_ip_and_port(char* ip_and_port, int* target_port, char* target_ip, int type) {
    //This function parse: type: 0:"xx.xx.xx.xx:xxx" 1:"xx,xx,xx,xx,xx,xx"
    remove_space(ip_and_port);
    if(type == 0) {
        char* colon = strchr(ip_and_port, ':');
        *target_port = atoi(colon + 1);
        *colon = 0;
    } else if (type == 1) {
        char* comma = strrchr(ip_and_port, ',');
        *target_port = atoi(comma + 1);
        *comma = 0;
        comma = strrchr(ip_and_port, ',');
        *target_port += (256 * atoi(comma + 1));
        *comma = 0;
        for(int i = 0; i < strlen(ip_and_port); ++i) {
            if(*(ip_and_port + i) == ',') {
                *(ip_and_port + i) = '.';
            }
        }
    }
    strcpy(target_ip, ip_and_port);
}

int start_with(const char* sentence, char* word) {
    for(int i = 0; i < strlen(word); ++i) {
        if(*(sentence + i) == '\0')
            return 0;
        if(*(sentence + i) != *(word + i))
            return 0;
    }
    return 1;
}

int send_command(int connfd, char *str) {
//    printf("Sending string:`%s`\n", (str));
    size_t len = strlen(str);
    int total_char = 0;
    while(total_char < len) {
        ssize_t n = send(connfd, str + total_char, len - total_char, 0);
        if (n == -1){
            printf("Error send(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        total_char += n;
    }
//    printf("Sent %d bytes successful!\n", total_char);
    return total_char;
}

int recv_command(int connfd, char *buffer, size_t len) {
    int total_char = 0;
    while(total_char < len){
//        printf("Waiting for trunk\n");
        ssize_t n = recv(connfd, buffer + total_char, len - total_char, 0);
        if(n == -1) {
            printf("Error recv(): %s(%d)\n", strerror(errno), errno);
            *buffer = '\0';
            return -1;
        } else if(n == 0) {
            break;
        }
//        printf("Received trunk of %d bytes\n", (int)n);
        total_char += n;
        if(*(buffer + total_char - 1) == '\n') {
            total_char --;
            buffer[total_char] = '\0';
            break;
        }
    }
//    printf("%s\n", buffer);
    return total_char;
}

void remove_space(char* buffer) {
    size_t i = strlen(buffer);
    while (i != 0) {
        switch(buffer[i - 1]) {
            case ' ':
            case '\r':
            case '\n':
            case '\t':
                i--;
                buffer[i] = '\0';
                break;
            default:
                i = 0;
                break;
        }
    }

    char* begin = buffer;
    while (*begin == ' ' || *begin == '\n' || *begin == '\r' || *begin == '\t') {
        begin += 1;
    }
    char temp_buffer[8192];
    strcpy(temp_buffer, begin);
    temp_buffer[strlen(begin)] = '\0';
    strcpy(buffer, temp_buffer);
}

void parse_command(char* buffer, char* command, char* parameter) {
    remove_space(buffer);
    char* buffer_inner_space_begin = strchr(buffer, ' ');
    if (buffer_inner_space_begin == NULL) {
        *parameter = '\0';
    } else {
        *buffer_inner_space_begin = '\0';
        strcpy(parameter, buffer_inner_space_begin + 1);
    }
    strcpy(command, buffer);
    remove_space(parameter);
}

void parse_ip(char* ip, int* ip_int_list) {
    char* period = ip;
    for(int i = 0; i < 3; ++i) {
        char* last_period = period;
        period = strchr(period + 1, '.');
        *period = '\0';
        ip_int_list[i] = atoi(last_period);
    }
    ip_int_list[3] = atoi(period + 1);
}

int socket_bind_listen(int *listenfd, int listening_port) {
    struct sockaddr_in addr;
    if ((*listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)listening_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(*listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Error bind(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    if (listen(*listenfd, 10) == -1) {
        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    printf("Listening on localhost:%d\n", listening_port);
    return 0;
}

int socket_connect_client(int* connectfd, int target_port, char* target_ip) {
    struct sockaddr_in server_addr;
    if ((*connectfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)target_port);
    if (inet_pton(AF_INET, target_ip, &server_addr.sin_addr) <= 0) {
        printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    if (connect(*connectfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error connect(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int send_file(int connfd, char* filename) {
    FILE* f = fopen(filename, "rb");
    if(!f) {
        printf("Error fopen(), filename: %s\n", filename);
        return 1;
    }
    char buf[8192];
    size_t n;
    while (1) {
        n = fread(buf, 1, 8190, f);
        if(send(connfd, buf, n, 0) < 0) {
            printf("Error send(), filename: %s\n", filename);
            fclose(f);
            return 2;
        }
        if(n <= 0)
            break;
    }
    fclose(f);
    return 0;
}

int recv_file(int connfd, char* filename) {
    FILE* f = fopen(filename, "wb");
    if(!f) {
        printf("Error fopen(), filename: %s\n", filename);
        return 1;
    }
    char buf[8192];
    ssize_t n;

    while (1) {
        if((n = recv(connfd, buf, 8190, 0)) < 0) {
            printf("Error recv(), filename: %s\n", filename);
            fclose(f);
            return 2;
        }
        fwrite(buf, 1, (size_t)n, f);
        if(n <= 0)
            break;
    }
    fclose(f);
    return 0;
}

