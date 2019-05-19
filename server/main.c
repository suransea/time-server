//
// Created by sea on 3/23/19.
//

#include "utility.h"
#include "thpool.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#define DEFAULT_PORT 5494
#define EPOLL_SIZE 4096
#define BUF_SIZE 8192
#define PROCESS_COUNT 5
#define THREAD_POOL_SIZE 20

void signal_handler(int sig);
void init_epoll(int *epoll_fd);
void init_socket(int epoll_fd, int *sock_fd);
void handle_accept(int epoll_fd, int sock_fd, thpool *pool);
void read_handler(void *arg);

static int quit_flag = 0;

struct worker_args {
  int epoll_fd;
  int sock_fd;
};

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    pid_t main_pid = getpid();
    for (int i = 1; i < PROCESS_COUNT; ++i) {
        if (getpid() == main_pid)fork();
    }
    thpool *pool = thpool_init(THREAD_POOL_SIZE);

    int epoll_fd;
    init_epoll(&epoll_fd);

    int listen_fd;
    init_socket(epoll_fd, &listen_fd);

    while (!quit_flag) {
        handle_accept(epoll_fd, listen_fd, pool);
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, listen_fd, NULL);
    close(listen_fd);
    close(epoll_fd);
    thpool_destroy(pool);
    return 0;
}

void signal_handler(int sig) {
    if (sig == SIGINT)quit_flag = 1;
}

/*初始化一个server socket*/
void init_socket(int epoll_fd, int *sock_fd) {
    *sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (*sock_fd == -1) {
        printf("[socket] create socket error: %s(errno: %d)\n", strerror(errno), errno);
        _exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DEFAULT_PORT);

    set_reuseport(*sock_fd);
    if (bind(*sock_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        printf("[bind] bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        _exit(1);
    }

    if (listen(*sock_fd, 1024) == -1) {
        printf("[listen] listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        _exit(1);
    }
    set_nonblock(*sock_fd);
    register_fd(epoll_fd, *sock_fd, false);
}

void init_epoll(int *epoll_fd) {
    *epoll_fd = epoll_create(EPOLL_SIZE);
    if (*epoll_fd < 0) {
        perror("[epoll_create] error\n");
        _exit(1);
    }
}

void handle_accept(int epoll_fd, int sock_fd, thpool *pool) {
    static struct epoll_event events[EPOLL_SIZE];
    int count = epoll_wait(epoll_fd, events, EPOLL_SIZE, -1);
    if (count < 0) {
        perror("[epoll_wait] error\n");
        return;
    }
    for (int i = 0; i < count; ++i) {
        int fd = events[i].data.fd;
        if (fd == sock_fd) {
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(struct sockaddr_in));
            socklen_t addr_len = sizeof(struct sockaddr_in);
            int connect_fd;
            while ((connect_fd = accept(sock_fd, (struct sockaddr *) &client_addr, &addr_len)) > 0) {
                printf("[accept] connection from: %s : % d(IP : port)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));
                set_nonblock(connect_fd);
                register_fd(epoll_fd, connect_fd, true);
            }
            if (connect_fd < 0 && errno != EAGAIN) {
                printf("[accept] accept socket error: %s errno :%d\n", strerror(errno), errno);
            }
        } else if (events[i].events & EPOLLIN) {
            struct worker_args *args = malloc(sizeof(struct worker_args));
            args->epoll_fd = epoll_fd;
            args->sock_fd = fd;
            thpool_add_work(pool, (void *) read_handler, args);
        }
    }
}

void read_handler(void *args) {
    struct worker_args *param = (struct worker_args *) args;
    int sock_fd = param->sock_fd;
    int epoll_fd = param->epoll_fd;
    free(param);
    char buf[BUF_SIZE];
    ssize_t len = recv_all(sock_fd, buf, BUF_SIZE, 0);
    reset_oneshot(epoll_fd, sock_fd);
    if (len <= 0)return;
    buf[len] = '\0';
    printf("[recv] %s\n", buf);
    if (strcmp(buf, "time") == 0) {
        time_t t;
        time(&t);
        char *time_str = ctime(&t);
        time_str[strlen(time_str) - 1] = '\0';
        if (send_all(sock_fd, time_str, strlen(time_str), 0) == -1) {
            perror("[send] send error\n");
            return;
        } else {
            printf("[send] %s\n", time_str);
        }
    }
    printf("[server] finish a request\n");
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, NULL);
    close(sock_fd);
}