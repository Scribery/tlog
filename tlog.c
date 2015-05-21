/*
 * Copyright (C) 2015 Red Hat
 *
 * This file is part of tlog.
 *
 * Tlog is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Tlog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tlog; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <pty.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <stdbool.h>
#include <poll.h>
#include <stdio.h>

enum fd_idx {
    FD_IDX_STDIN,
    FD_IDX_MASTER,
    FD_IDX_NUM
};

int
main(int argc, char **argv)
{
    ssize_t rc;
    int master_fd;
    pid_t child_pid;
    struct termios orig_termios;
    struct termios raw_termios;
    struct winsize winsize;
    struct pollfd fds[FD_IDX_NUM];
    char buf[1024];
    ssize_t size;

    (void)argc;
    (void)argv;

    /* Get terminal attributes */
    rc = tcgetattr(STDIN_FILENO, &orig_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed retrieving tty attributes: %s",
                strerror(errno));
        return 1;
    }

    /* Get terminal window size */
    rc = ioctl(STDIN_FILENO, TIOCGWINSZ, &winsize);
    if (rc < 0) {
        fprintf(stderr, "Failed retrieving tty window size: %s",
                strerror(errno));
        return 1;
    }

    /* Fork a child under a slave pty */
    child_pid = forkpty(&master_fd, NULL, &orig_termios, &winsize);
    if (child_pid < 0) {
        fprintf(stderr, "Failed forking a pty: %s", strerror(errno));
        return 1;
    } else if (child_pid == 0) {
        /*
         * Child
         */
        execl("/bin/bash", "/bin/bash", NULL);
        return 0;
    }

    /*
     * Parent
     */

    /* Switch the terminal to raw mode */
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    raw_termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                             INPCK | ISTRIP | IXON | PARMRK);
    raw_termios.c_oflag &= ~OPOST;
    raw_termios.c_cc[VMIN] = 1;
    raw_termios.c_cc[VTIME] = 0;

    rc = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed setting tty attributes: %s", strerror(errno));
        return 1;
    }

    /* Transfer I/O */
    fds[FD_IDX_STDIN].fd        = STDIN_FILENO;
    fds[FD_IDX_STDIN].events    = POLLIN;
    fds[FD_IDX_MASTER].fd       = master_fd;
    fds[FD_IDX_MASTER].events   = POLLIN;
    while (true) {
        rc = poll(fds, sizeof(fds) / sizeof(fds[0]), -1);
        if (rc < 0) {
            fprintf(stderr, "Failed waiting for I/O: %s", strerror(errno));
            break;
        }
        if (fds[FD_IDX_STDIN].revents & (POLLIN | POLLHUP | POLLERR)) {
            rc = read(STDIN_FILENO, buf, sizeof(buf));
            if (rc <= 0)
                break;
            size = rc;
            rc = write(master_fd, buf, size);
            if (rc != size) {
                fprintf(stderr, "Failed/partial write to master: %s",
                        strerror(errno));
                break;
            };
        }
        if (fds[FD_IDX_MASTER].revents & (POLLIN | POLLHUP | POLLERR)) {
            rc = read(master_fd, buf, sizeof(buf));
            if (rc <= 0)
                break;
            size = rc;
            rc = write(STDOUT_FILENO, buf, size);
            if (rc != size) {
                fprintf(stderr, "Failed/partial write to stdout: %s",
                        strerror(errno));
                break;
            };
        }
    }

    /* Restore terminal attributes */
    rc = tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    if (rc < 0) {
        fprintf(stderr, "Failed restoring tty attributes: %s", strerror(errno));
        return 1;
    }

    return 0;
}
