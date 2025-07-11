/*
 * Common functions header
 *
 * Author: Adam Debek
 */

#include <stddef.h>

#ifndef _COMMON_H
#define _COMMON_H

const char *sendall(int sockfd, const char *buffer, size_t length, int flags);

#endif /* _COMMON_H */
