#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
	int cfd;
	int fd;
	struct sockaddr_in si;
	const char *request = "That’s it. You’re welcome.";
	socklen_t si_len = sizeof(si);

	fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0) {
                fprintf(stderr, "error in creating socket\n");
                exit(-1);
        }

	si.sin_family = PF_INET;
	inet_aton("172.16.1.3", &si.sin_addr);
	si.sin_port = htons(1025);

	if (bind(fd, (struct sockaddr*)&si, sizeof si) < 0) {
		perror("bind");
	}

	if (listen(fd, 10000) < 0 ) {
		perror("listen");
	}

	while ( (cfd = accept(fd, (struct sockaddr *)&si, &si_len)) != -1) {
		if (fork() == 0) {
			char buf[4096] = { 0 };
			while( read(cfd, buf, sizeof buf) != 0) {
				write(cfd, request, strlen(request) + 1);
				write(cfd, request, strlen(request) + 1);
			}
		}
	}

	close(fd);

	return 0;
}

