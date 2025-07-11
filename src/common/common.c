/*
 * Common functions header
 *
 * Author: Adam Debek
 */

#include <common.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

const char *sendall(int sockfd, const char *buffer, size_t length, int flags)
{
    ssize_t n = 0;
    size_t total = 0;
    size_t left = length;
    size_t send_len = length;
    struct pollfd fds[1];

    fds[0].fd = sockfd;
    fds[0].events = POLLOUT;
    fds[0].revents = 0;

    while (n < length) {
        int ret;

        ret = poll(fds, 1, 5000);
        if (ret == 0) {
            return "sendall(): poll() timeout";
        }

        if (fds[0].revents & POLLOUT) {
            n = send(sockfd, buffer + total, send_len, flags);
            if (n > 0) {
                total += n;
                left -= n;
                if (left < send_len) {
                    send_len = left;
                }
            }
            else if (n < 0 && errno == EPIPE) {
                return "sendall(): Server closed the connection";
            }
            else if (n < 0 && errno == EMSGSIZE) {
                send_len /= 2;
                continue;
            }
            else if (n < 0) {
                return "sendall(): Internal send() error";
            }
        }
        else {
            return "sendall(): Connection hangup";
        }
    }

    if (n == length) {
        return (const char*)NULL;
    }
    else if (n > length) {
        return "sendall(): Too many bytes was sent";
    }
    else {
        return "sendall(): Too few bytes was sent";
    }
}
