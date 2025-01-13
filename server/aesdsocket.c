#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>

#define INITIAL_BUF_SIZE        (1024)
#define PORT                    "9000"
#define TMP_FILE_NAME           "/var/tmp/aesdsocketdata"
#define SERVER_EXIT_OK          (0)
#define SERVER_EXIT_ERR         (-1)

typedef struct {
    int filefd;                 // Log file descriptor for writing received packets
    int sockfd;                 // Server socket file descriptor
    struct addrinfo *servinfo;  // Server address info
    char buf[INITIAL_BUF_SIZE]; // Buffer for receiving data from client
    char client_ip[INET6_ADDRSTRLEN]; // Client IP address
} server_data_t;

server_data_t g_server_data = {
    .filefd = -1,
    .sockfd = -1,
    .servinfo = NULL,
};

/* Signal handler to catch SIGINT and SIGTERM */
void server_shutdown(int sig) {
    int exit_code = SERVER_EXIT_OK;
    if (sig == SERVER_EXIT_ERR) {
        exit_code = SERVER_EXIT_ERR;
    } else {
        syslog(LOG_DEBUG, "Caught signal, exiting");
    }

    if (g_server_data.filefd != -1) {
        close(g_server_data.filefd);
    }
    
    if (g_server_data.sockfd != -1) {
        shutdown(g_server_data.sockfd, SHUT_RDWR);
    }

    freeaddrinfo(g_server_data.servinfo);
    // delete the temp file
    remove(TMP_FILE_NAME);
    exit(exit_code);
}

/* Register signal handler for SIGINT and SIGTERM */
void register_signal() {
    signal(SIGTERM, server_shutdown);
    signal(SIGINT, server_shutdown);
}

/* Open the file for writing received packets */
void open_file(server_data_t *server_data) {
    server_data->filefd = open(TMP_FILE_NAME, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (server_data->filefd == -1) {
        perror("open");
        server_shutdown(SERVER_EXIT_ERR);
    }
}

/* Fill the client IP address */
void fill_client_ip(struct sockaddr_storage *their_addr, server_data_t *server_data)
{
    if (their_addr->ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)their_addr;
        inet_ntop(AF_INET, &s->sin_addr, server_data->client_ip, sizeof(server_data->client_ip));
    } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)their_addr;
        inet_ntop(AF_INET6, &s->sin6_addr, server_data->client_ip, sizeof(server_data->client_ip));
    }
}

/*  Send the file data to the client */
void send_file_to_client(server_data_t *server_data, int new_fd)
{
    lseek(server_data->filefd, 0, SEEK_SET);
    while(1) {
        int numbytes = read(server_data->filefd, server_data->buf, sizeof(server_data->buf));
        if (numbytes == -1) {
            perror("read");
            server_shutdown(SERVER_EXIT_ERR);
        } else if (numbytes == 0) {
            break;
        } else {
            if (send(new_fd, server_data->buf, numbytes, 0) == -1) {
                perror("send");
                server_shutdown(SERVER_EXIT_ERR);
            }
        }
    }
}

/* Listen to the client and write the received data to the file */
void listen_to_client(server_data_t *server_data)
{
    listen(server_data->sockfd, 1);
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);
    int new_fd = accept(server_data->sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
        perror("accept");
        server_shutdown(SERVER_EXIT_ERR);
    } else {
        // Get the client IP address:
        fill_client_ip(&their_addr, server_data);
        syslog(LOG_DEBUG, "Accepted connection from %s\n", server_data->client_ip);
        while(1) {
            int numbytes = recv(new_fd, server_data->buf, sizeof(server_data->buf), 0);
            if (numbytes == -1) {
                perror("recv");
                server_shutdown(SERVER_EXIT_ERR);
            } else if (numbytes){
                server_data->buf[numbytes] = '\0';
                // Write the received data to the file
                if (write(server_data->filefd, server_data->buf, numbytes) == -1) {
                    perror("write");
                    server_shutdown(SERVER_EXIT_ERR);
                }
                if (server_data->buf[numbytes-1] == '\n') {
                    // send all collected data back to the client.
                    send_file_to_client(server_data, new_fd);
                }
            } else {
                syslog(LOG_DEBUG, "Closed connection from %s\n", server_data->client_ip);
                break;
            }
        }
    }
}

/* Open the socket for listening to the client */
void open_socket(server_data_t *server_data) {
    int sockfd;
    struct addrinfo hints, *p;
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &server_data->servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        server_shutdown(SERVER_EXIT_ERR);
    }
    // loop through all the results and bind to the first we can
    for(p = server_data->servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }
        break; // if we get here, we must have connected successfully
    }
    if (p == NULL) {
        // looped off the end of the list with no successful bind
        fprintf(stderr, "failed to bind socket\n");
        server_shutdown(SERVER_EXIT_ERR);
    }
    server_data->sockfd = sockfd;
    const int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed\n");
        server_shutdown(SERVER_EXIT_ERR);
    }
}


int main(int argc, char const *argv[])
{
    openlog(NULL, 0, LOG_USER);
    register_signal();
    open_file(&g_server_data);
    open_socket(&g_server_data);
    // if first argument is -d, run as daemon
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(-1);
        } else if (pid > 0) {
            exit(0);
        }
    }
    while(1) {
        listen_to_client(&g_server_data);
    }

    server_shutdown(SIGINT);
}