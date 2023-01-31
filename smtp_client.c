#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int parse_smtp_response(ssize_t bytes_read, char *buffer)
{
    int response_code = strtol(buffer, NULL, 10);
    if (response_code == 354) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) 
{
    char buffer[BUFFER_SIZE];
    char *user_input = NULL;
    in_addr_t in_addr;
    in_addr_t server_addr;

    size_t getline_buffer = 0;
    ssize_t bytes_read, i, user_input_len;
    int sockfd;
    struct hostent *hostent;
    struct sockaddr_in sockaddr_in;

    unsigned short server_port = 25;
    char *server_hostname = "smtp.gmail.com";

    if (argc != 3) {
        fprintf(stderr, "./a.out <smtp server ip> <port>\n");
        exit(EXIT_FAILURE);
    }

    server_hostname = argv[1];
    server_port = strtol(argv[2], NULL, 10);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Failed to create socket (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    hostent = gethostbyname(server_hostname);
    if (hostent == NULL) {
        fprintf(stderr, "Failed to gethostbyname(\"%s\") (%d)\n", server_hostname, errno);
        exit(EXIT_FAILURE);
    }

    in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    if (in_addr == (in_addr_t)-1) {
        fprintf(stderr, "Failed to get inet_addr(\"%s\") (%d)\n", *(hostent->h_addr_list), errno);
        exit(EXIT_FAILURE);
    }

    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(server_port);

    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
        fprintf(stderr, "Failed to connect (%d)\n", errno);
        return EXIT_FAILURE;
    }

    /* Read initial response - should be similar to 220 smtp.gmai.com ESMTP .... */
    memset(buffer, '\0', BUFFER_SIZE);
    bytes_read = read(sockfd, buffer, BUFFER_SIZE);
    buffer[bytes_read] = '\n';
    fprintf(stdout, "%s\n", buffer);
    fflush(stdout);

    while (1) {
        memset(buffer, '\0', BUFFER_SIZE);
        user_input_len = getline(&user_input, &getline_buffer, stdin);
        if (user_input_len == -1) {
            fprintf(stderr, "Failed reading user input (%d)\n", errno);
            exit(EXIT_FAILURE);
        }
        if (user_input_len == 1) {
            close(sockfd);
            break;
        }
        if (write(sockfd, user_input, user_input_len) == -1) {
            fprintf(stderr, "Failed to write to socket (%d)\n", errno);
            exit(EXIT_FAILURE);
        }

        bytes_read = read(sockfd, buffer, BUFFER_SIZE);
        buffer[bytes_read] = '\n';
        fprintf(stdout, "%s\n", buffer);
        fflush(stdout);

        if (parse_smtp_response(bytes_read, buffer)) {
            while(1) {
                user_input_len = getline(&user_input, &getline_buffer, stdin);
                if (write(sockfd, user_input, user_input_len) == -1) {
                    fprintf(stderr, "Failed to write to socket (%d)\n", errno);
                    exit(EXIT_FAILURE);
                }
                if (strcmp(user_input, "."))
                    break;
            }
        }
    }

    free(user_input);
    exit(EXIT_SUCCESS);
}
