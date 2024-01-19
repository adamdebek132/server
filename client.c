#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <libgen.h>


#define CONN_ERR 1
#define SEND_ERR 2

// ANSI escape codes for text colors
#define ANSI_COLOR_RED   "\x1b[31m"
#define ANSI_COLOR_RESET "\x1b[0m"


#define ERR_PRINTF(message) \
    do { \
        fprintf(stderr, ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", message); \
    } while(0)


#define FUNC_ERR_PRINTF(message) \
    do { \
        fprintf(stderr, ANSI_COLOR_RED "Error at %s:%d: %s" ANSI_COLOR_RESET "\n", __FILE__, __LINE__ - 1, message); \
    } while(0)


static void print_usage(char *prog_path)
{
	char *prog_name;
	prog_name = basename(prog_path);

	fprintf(stderr, "Usage: %s <clients_num>\n", prog_name);
	_exit(EXIT_FAILURE);
}


/* Get random value from range (min,max) */
static unsigned int get_random(unsigned int min, unsigned int max)
{
	unsigned int range = max - min + 1;
	unsigned int random_value = rand() % range + min;

	return random_value;
}

char buf[4096];
int len, err, i, client_num, fd;
int connect_fd[1000];
struct sockaddr_in si;
pid_t pid;
const char *request = "GET / HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "User-Agent: SimpleHTTPClient\r\n"
                          "Connection: close\r\n\r\n";

int main(int argc, char **argv)
{
	if (argc != 2) {
        	print_usage(argv[0]);
	}

	memset(&si, 0, sizeof si);
	si.sin_family= PF_INET;
	inet_aton("172.16.1.3", &si.sin_addr);
	si.sin_port = htons(1025);

	client_num = atoi(argv[1]);

	for (i = 0; i < client_num; i++) {
		if ( (pid = fork()) < 0) {
			FUNC_ERR_PRINTF(strerror(errno));
		} else if (pid == 0) {
			/* Time in ms after next request will be sent */
			unsigned int sndreq_sleep;
			struct timeval start, end;
			double elapsed_time;
			pid_t my_pid;

			my_pid = getpid();

			fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (fd < 0) {
 				FUNC_ERR_PRINTF(strerror(errno));
 				exit(EXIT_FAILURE);
			}

			if (connect(fd, (struct sockaddr *)&si, sizeof si) < 0) {
               			FUNC_ERR_PRINTF(strerror(errno));
        		}

			while(1) {
				sndreq_sleep = get_random(1e3, 2e6);
				usleep(sndreq_sleep);

				gettimeofday(&start, NULL);
				if (send(fd, request, strlen(request), MSG_NOSIGNAL) < 0) {
					FUNC_ERR_PRINTF(strerror(errno));
        			}

				//len = read(fd, buf, sizeof buf);

				gettimeofday(&end, NULL);
				elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

				printf("%d response: %fs\n", my_pid - getppid(), elapsed_time);
			}
		}
	}
	wait(NULL);

	close(fd);

	return 0;
}
