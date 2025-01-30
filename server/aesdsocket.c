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
#include <pthread.h>

#define INITIAL_BUF_SIZE        (1024)
#define PORT                    "9000"
#define TMP_FILE_NAME           "/var/tmp/aesdsocketdata"
#define SERVER_EXIT_OK          (0)
#define SERVER_EXIT_ERR         (-1)

#define LOG_DISABLE             (0)
#define LOG_VIA_SYSLOG          (1)
#define LOG_VIA_STDOUT          (2)

#define DEFAULT_LOGGING         (LOG_DISABLE)
#if DEFAULT_LOGGING == LOG_DISABLE
#define DEBUG_LOG(msg,...)
#define ERROR_LOG(msg,...)
#elif DEFAULT_LOGGING == LOG_VIA_SYSLOG
#define DEBUG_LOG(msg,...) syslog(LOG_DEBUG, "socket DEBUG: " msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg,...) syslog(LOG_ERR, "socket ERROR: " msg "\n", ##__VA_ARGS__)
#elif DEFAULT_LOGGING == LOG_VIA_STDOUT
#define DEBUG_LOG(msg,...) printf("socket DEBUG: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("socket ERROR: " msg "\n" , ##__VA_ARGS__)
#endif
// Linked list to store the thread ids
typedef struct thread_list {
    pthread_t thread_id;
    void * thread_data;
    struct thread_list *next;
} thread_list_t;

typedef struct {
    int filefd;                 // Log file descriptor for writing received packets
    int sockfd;                 // Server socket file descriptor
    struct addrinfo *servinfo;  // Server address info
    thread_list_t   thread_list;
    pthread_mutex_t mutex;
} server_data_t;

server_data_t g_server_data = {
    .filefd = -1,
    .sockfd = -1,
    .servinfo = NULL,
    .thread_list = {0},
};
typedef struct {
    server_data_t *p_server_data;       // Pointer to the server data
    int  new_fd;
    struct sockaddr_storage their_addr; // Client address
    char buf[INITIAL_BUF_SIZE];         // Buffer for receiving data from client
    char client_ip[INET6_ADDRSTRLEN];   // Client IP address
}thread_data_t;

void join_all_threads()
{
    thread_list_t *temp = &g_server_data.thread_list;
    thread_list_t *next = NULL;
    thread_data_t *thread_data = NULL;
    while(temp->next != NULL)
    {
        DEBUG_LOG("Joining thread 0x%lx", temp->next->thread_id);
        pthread_join(temp->next->thread_id, NULL);
        next = temp->next->next;
        thread_data = temp->next->thread_data;
        DEBUG_LOG("Freeing thread node thread_data at %p", temp->thread_data);
        free(temp->thread_data);
        DEBUG_LOG("Freeing thread node at %p", temp->next);
        free(temp->next);
        temp->next = next;
        temp->thread_data = thread_data;
    }
}

/* Signal handler to catch SIGINT and SIGTERM */
void server_shutdown(int sig) {
    int exit_code = SERVER_EXIT_OK;
    if (sig == SERVER_EXIT_ERR) {
        exit_code = SERVER_EXIT_ERR;
    } else {
        DEBUG_LOG("Caught signal, exiting");
    }

    if (g_server_data.filefd != -1) {
        close(g_server_data.filefd);
    }
    
    if (g_server_data.sockfd != -1) {
        shutdown(g_server_data.sockfd, SHUT_RDWR);
    }
    // close all the threads
    join_all_threads();
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
void fill_client_ip(struct sockaddr_storage *their_addr, thread_data_t *thread_data)
{
    if (their_addr->ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)their_addr;
        inet_ntop(AF_INET, &s->sin_addr, thread_data->client_ip, sizeof(thread_data->client_ip));
    } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)their_addr;
        inet_ntop(AF_INET6, &s->sin6_addr, thread_data->client_ip, sizeof(thread_data->client_ip));
    }
}

/*  Send the file data to the client */
void send_file_to_client(thread_data_t *thread_data)
{
    lseek(thread_data->p_server_data->filefd, 0, SEEK_SET);
    while(1) {
        int numbytes = read(thread_data->p_server_data->filefd, thread_data->buf, sizeof(thread_data->buf));
        if (numbytes == -1) {
            perror("read");
            server_shutdown(SERVER_EXIT_ERR);
        } else if (numbytes == 0) {
            break;
        } else {
            if (send(thread_data->new_fd, thread_data->buf, numbytes, 0) == -1) {
                perror("send");
                server_shutdown(SERVER_EXIT_ERR);
            }
        }
    }
}

void* threadfunc(void* thread_param)
{
    thread_data_t* thread_func_args = (thread_data_t *) thread_param;
    // Get the client IP address:
    fill_client_ip(&thread_func_args->their_addr, thread_func_args);
    DEBUG_LOG("Accepted connection from %s", thread_func_args->client_ip);
    while(1) {
        int numbytes = recv(thread_func_args->new_fd, thread_func_args->buf, sizeof(thread_func_args->buf), 0);
        if (numbytes == -1) {
            perror("recv");
            server_shutdown(SERVER_EXIT_ERR);
        } else if (numbytes) {
            thread_func_args->buf[numbytes] = '\0';
            // Write the received data to the file
            pthread_mutex_lock(&thread_func_args->p_server_data->mutex);
            if (write(thread_func_args->p_server_data->filefd, thread_func_args->buf, numbytes) == -1) {
                perror("write");
                server_shutdown(SERVER_EXIT_ERR);
            }
            pthread_mutex_unlock(&thread_func_args->p_server_data->mutex);
            if (thread_func_args->buf[numbytes-1] == '\n') {
                // send all collected data back to the client.
                send_file_to_client(thread_func_args);
            }
        } else {
            DEBUG_LOG("Closed connection from %s", thread_func_args->client_ip);
            break;
        }
    }

    return thread_param;
}

void add_thread_to_list(pthread_t thread_id, thread_data_t *thread_data)
{
    thread_list_t *thread_node = malloc(sizeof(thread_list_t));
    DEBUG_LOG("Thread node allocated at %p", thread_node);
    if(thread_node == NULL)
    {
        ERROR_LOG("Failed to allocate memory for thread_node");
        server_shutdown(SERVER_EXIT_ERR);
    }
    DEBUG_LOG("Adding thread 0x%lx to list", thread_id);
    thread_node->thread_id = thread_id;
    thread_node->next = NULL;
    thread_node->thread_data = NULL;
    thread_list_t *temp = &g_server_data.thread_list;
    while(temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = thread_node;
    temp->thread_data = thread_data;
}

/* Listen to the client and write the received data to the file */
void listen_to_client(server_data_t *server_data)
{
    listen(server_data->sockfd, 1);
    struct sockaddr_storage their_addr = {0};
    socklen_t addr_size = sizeof(their_addr);
    int new_fd = accept(server_data->sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
        perror("accept");
        server_shutdown(SERVER_EXIT_ERR);
    } else {
        DEBUG_LOG("Spawning thread to handle client");
        // Spawn a new thread to handle the client
        pthread_t thread;
        thread_data_t *thread_data = malloc(sizeof(thread_data_t));
        DEBUG_LOG("Thread data allocated at %p", thread_data);
        if(thread_data == NULL)
        {
            ERROR_LOG("Failed to allocate memory for thread_data");
            server_shutdown(SERVER_EXIT_ERR);
        }
        thread_data->new_fd = new_fd;
        thread_data->p_server_data = server_data;
        thread_data->their_addr = their_addr;
        if(pthread_create(&thread, NULL, threadfunc, thread_data) != 0)
        {
            ERROR_LOG("Failed to create thread");
            free(thread_data);
            server_shutdown(SERVER_EXIT_ERR);
        }

        add_thread_to_list(thread, thread_data);
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

void write_timestamp_to_file(union sigval sigval) {
    server_data_t *server_data = (server_data_t *)sigval.sival_ptr;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf)-1, "timestamp: %Y-%m-%d %H:%M:%S\n", t);
    DEBUG_LOG("Writing timestamp %s to file.", buf);
    pthread_mutex_lock(&server_data->mutex);
    if (write(server_data->filefd, buf, strlen(buf)) == -1) {
        perror("write");
        server_shutdown(SERVER_EXIT_ERR);
    }
    pthread_mutex_unlock(&server_data->mutex);
}

void start_timer(server_data_t *server_data) {
    struct sigevent sev;
    struct itimerspec its;
    timer_t timerid;

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = server_data;
    sev.sigev_notify_function = write_timestamp_to_file;
    sev.sigev_notify_attributes = NULL;

    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
        perror("timer_create");
        server_shutdown(SERVER_EXIT_ERR);
    }

    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 10;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
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
    // Start a timer with 10 seconds interval which calls a function write a timestamp to the file
    start_timer(&g_server_data);

    while(1) {
        listen_to_client(&g_server_data);
    }

    server_shutdown(SIGINT);
}