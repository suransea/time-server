//
// Created by sea on 3/23/19.
//

#ifndef NETWORK2_UTILITY_H
#define NETWORK2_UTILITY_H

#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>

ssize_t recv_all(int __fd, void *__buf, size_t __n, int __flags) {
    size_t nread = 0;
    ssize_t len;
    while ((len = recv(__fd, __buf + nread, __n - nread, __flags)) > 0) {
        nread += len;
    }
    if (len == -1 && errno != EAGAIN) {
        return -1;
    }
    return nread;
}

ssize_t send_all(int __fd, const void *__buf, size_t __n, int __flags) {
    size_t nsent = 0;
    while (nsent < __n) {
        ssize_t len = send(__fd, __buf + nsent, __n - nsent, __flags);
        if (len < 0) {
            if (errno == EINTR)return -1;
            if (errno == EAGAIN) {
                usleep(1000);
                continue;
            }
            return -1;
        }
        nsent += len;
    }
    return nsent;
}

void set_nonblock(int sock_fd) {
    fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFD, 0) | O_NONBLOCK);
}

void set_reuseport(int sock_fd) {
    int val = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)) < 0) {
        perror("[set_reuseport] error");
        _exit(1);
    }
}

void reset_oneshot(int epoll_fd, int fd) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void register_fd(int epoll_fd, int fd, bool oneshot) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    if (oneshot)ev.events |= EPOLLONESHOT;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

#endif //NETWORK2_UTILITY_H
