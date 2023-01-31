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

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE       1024
#define DATA_RESPONSE     354
#define STARTTLS_RESPONSE 220

#define TLS_ERROR_STRING ERR_reason_error_string(ERR_get_error())

typedef enum {
    INVALID_RESPONSE,
    READY_FOR_STARTTLS,
    READY_FOR_DATA,
} smtp_response;

smtp_response parse_smtp_response(ssize_t bytes_read, char *buffer)
{
    int response_code = strtol(buffer, NULL, 10);
    switch (response_code) {
    case STARTTLS_RESPONSE:
        return READY_FOR_STARTTLS;
        break;

    case DATA_RESPONSE:
        return READY_FOR_DATA;
        break;

    default:
        return INVALID_RESPONSE;
        break;
    }
}

/*
 * Configure SSL CTX - Set the min and max TLS protocols to TLSv1.2 and TLSv1.3
 *
 */
SSL_CTX *configure_tls()
{
    SSL_load_error_strings();
    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());

    if (ssl_ctx == NULL) {
        fprintf(stderr, "Failed to configure SSL CTX (%s)\n", TLS_ERROR_STRING); 
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION) == 0) {
        fprintf(stderr, "Faile to set minimum TLS version (%s)\n", TLS_ERROR_STRING); 
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION) == 0) {
        fprintf(stderr, "Failed tp set maximum TLS version (%s)\n", TLS_ERROR_STRING); 
        exit(EXIT_FAILURE);
    }

    return ssl_ctx;
}

/*
 * Read from the server
 *
 */
int smtp_read(int sockfd, SSL *ssl, char *buffer, int len)
{
    ssize_t bytes_read = 0;
    if (ssl != NULL) {
        bytes_read = SSL_read(ssl, buffer, len); 
    }
    else {
        bytes_read = read(sockfd, buffer, len);
    }

    if (bytes_read >  0) {
        buffer[bytes_read] = '\0';
        fprintf(stdout, "%s\n", buffer);
        fflush(stdout);
    }
    return bytes_read;
}

/*
 * Write to the server
 *
 */
int smtp_write(int sockfd, SSL *ssl, char *buffer, int len)
{
    ssize_t bytes_written = 0;
    char buf[BUFFER_SIZE + 2];

    snprintf(buf, len, "%s", buffer);
    strcat(buf, "\r\n");

    if (ssl != NULL) {
        bytes_written = SSL_write(ssl, buf, strlen(buf));
    }
    else {
        bytes_written = write(sockfd, buf, strlen(buf));
    }
}

int main(int argc, char **argv) 
{
    char buffer[BUFFER_SIZE];
    char *user_input = NULL;
    in_addr_t in_addr;
    in_addr_t server_addr;

    smtp_response smtp_res = INVALID_RESPONSE;

    size_t getline_buffer = 0;
    ssize_t bytes_read, i, user_input_len;
    int sockfd;
    struct hostent *hostent;
    struct sockaddr_in sockaddr_in;

    unsigned short server_port = 25;
    char *server_hostname = "smtp.gmail.com";

    SSL_CTX *ssl_ctx = NULL;
    SSL *ssl = NULL;

    if (argc != 3) {
        fprintf(stderr, "./a.out <smtp server ip> <port>\n");
        exit(EXIT_FAILURE);
    }

    server_hostname = argv[1];
    server_port = strtol(argv[2], NULL, 10);

    ssl_ctx = configure_tls();

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
    if (smtp_read(sockfd, ssl, buffer, BUFFER_SIZE) < 0) {
        fprintf(stderr, "Failed to read (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    while (1) {
        memset(buffer, '\0', BUFFER_SIZE);
        user_input_len = getline(&user_input, &getline_buffer, stdin);
        if (user_input_len == -1) {
            fprintf(stderr, "Failed reading user input (%d)\n", errno);
            exit(EXIT_FAILURE);
        }

        /* If quit, then quit */
        if (strncmp(user_input, "quit", 4) == 0) {
            close(sockfd);
            break;
        }

        if (smtp_write(sockfd, ssl, user_input, user_input_len) < 0) {
            fprintf(stderr, "Failed to write to socket (%d) (%s)\n", errno, (ssl != NULL) ? TLS_ERROR_STRING : "");
            exit(EXIT_FAILURE);
        }

        if (smtp_read(sockfd, ssl, buffer, BUFFER_SIZE) < 0) {
            fprintf(stderr, "Failed to read (%d) (%s)\n", errno, (ssl != NULL) ? TLS_ERROR_STRING : "");
            exit(EXIT_FAILURE);
        }

        smtp_res = parse_smtp_response(bytes_read, buffer);
        if (smtp_res == READY_FOR_DATA) {
            while(1) {
                user_input_len = getline(&user_input, &getline_buffer, stdin);
                if (strncmp(user_input, ".", 1) == 0) {
                    smtp_write(sockfd, ssl, ".", 1);
                    break;
                }
                else if (strncmp(user_input, "quit", 4) == 0) {
                    close(sockfd);
                    break;
                }

                if (smtp_write(sockfd, ssl, user_input, user_input_len) < 0) {
                    fprintf(stderr, "Failed to write to socket (%d) (%s)\n", errno, TLS_ERROR_STRING);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (smtp_res == READY_FOR_STARTTLS) {
            ssl = SSL_new(ssl_ctx);
            if (ssl == NULL) {
                fprintf(stderr, "Failed to create new SSL context (%s)\n", TLS_ERROR_STRING);
                exit(EXIT_FAILURE);
            }

            SSL_set_fd(ssl, sockfd);
            if (SSL_connect(ssl) < 0) {
                fprintf(stderr, "Failed to SSL_connect (%s)\n", TLS_ERROR_STRING);
                exit(EXIT_FAILURE);
            }
        }
    }

    smtp_read(sockfd, ssl, buffer, BUFFER_SIZE);
    free(user_input);
    exit(EXIT_SUCCESS);
}
