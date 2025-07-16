/*
 * Http server benchmark
 *
 * Author: Adam Debek
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <common.h>


#define MAX_CLIENTS 10000u
#define MAX_REQS    1000000u
#define MAX_FILES   1000u
#define MAX_EXENTS  10000

typedef struct client_results {
	unsigned int time;
} c_results_t;

typedef struct client {
	char *files[MAX_FILES]; /* Paths to files client will request */
	unsigned int req_num;   /* Number of request client will make */
	int interv;             /* Time between requests in ms, -1 if random */
	int max_latency;        /* Maximum acceptable latency of data retrival in ms, -1 if no latency check */
	c_results_t *results;   /* Pointer to memory where thread results are stored */
} client_t;

/* ------------------------ GLOBALS ------------------------ */

static unsigned int verbosity = 0;     /* No verbosity on default */
static unsigned int is_quiet = 0;      /* Quiet run flag */
static const client_t default_client = {
	.files = {"/", NULL},
	.req_num = 10,
	.interv = 50,
	.max_latency = -1
}; /* Default client type */
static unsigned int client_num = 1;     /* Number of concurrent clients */
static unsigned int request_num = 0;    /* Number of all requests */
static unsigned int completed_reqs = 0; /* All completed requests */
static unsigned int failed_reqs = 0;    /* All failed requests */
static char ifname[IFNAMSIZ];           /* Name of network interface */
static char ifaddr[INET_ADDRSTRLEN];    /* IPv4 address of interface */
static char hostname[33];               /* Name of server */
static unsigned int server_port = 0;    /* Server port to connect to */
static char log_file[64];               /* Log file name */

/* ------------------------ REQUESTS ----------------------- */

static const char *request_header = "User-Agent: Adam-Benchmark\r\n"
                                    "Connection: close\r\n"
                                    "\r\n";


static const char *create_request(const char *method, const char *uri,
                                  const char *header, const char *body)
{
	char *request = (char *)malloc(4096 * sizeof(char));
	int n = 0;

	if (request == NULL) {
		return NULL;
	}

	if (method == NULL || uri == NULL) {
		free(request);
		return NULL;
	}

	n += sprintf(request, "%s %s HTTP/0.9\r\n", method, uri);
	n += sprintf(request + n, "Host: %s\r\n", hostname);
	if (header != NULL) {
		n += sprintf(request + n, "%s", header);
	}
	if (body != NULL) {
		sprintf(request + n, "%s\r\n", body);
	}

	return request;
}


static void print_usage(char *prog_path)
{
	printf("Usage: %s <hostname:port> [options]\n", basename(prog_path));
	printf("Options:\n");
	printf("\t-n: number of request made by all concurrent connections\n");
	printf("\t-c: number of concurrent connections\n");
	printf("\t-i: interface name to which traffic is routed\n");
	printf("\t-v: enable verbose mode\n");
	printf("\t-q: enable quiet mode\n");
	printf("\t-h: print help\n");
}


static int getifaddr(char *ifname, char *ifaddr)
{
	struct ifreq ifr;
	short if_flags;
	int fd;

	if (ifname == NULL) {
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		close(fd);
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1) {
		if (errno == ENODEV) {
			fprintf(stderr, "No such interface: %s\n", ifname);
		} else {
			perror("ioctl");
		}
		close(fd);
		return -1;
	}

	if_flags = ifr.ifr_flags;
	if ((if_flags & IFF_RUNNING) == 0) {
		fprintf(stderr, "Interface %s is not running\n", ifname);
		close(fd);
		return -1;
	}

	if (ioctl(fd, SIOCGIFADDR, &ifr) == -1 && errno == EADDRNOTAVAIL) {
		fprintf(stderr, "You need to assign IP address to %s\n", ifname);
		close(fd);
		return -1;
	}

	close(fd);

	if (inet_ntop(AF_INET,
	              &((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr,
	              ifaddr,
	              INET_ADDRSTRLEN) == NULL) {
		perror("inet_ntop");
		return -1;
	}

	return 0;
}


static int parse_addr(const char* arg)
{
	if (arg == NULL) {
		return -1;
	}

	/* 39 = hostname (max 32 bytes) + delimiter ':'(max 1 byte) + port (max 5 bytes) + '\0' (1 byte) */
	char addr[39];
	strcpy(addr, arg);

	int err = 0;
	size_t addr_len = strlen(addr);
	size_t hostname_len, port_len;

	char *base_hostname = strtok(addr, ":");
	char *base_port = strtok(NULL, "");

	if (base_port == NULL) {
		/* Wrong address, empty hostname or port */
		return -2;
	}

	char *end = base_hostname + addr_len;
	hostname_len = (base_port - 1) - base_hostname;
	port_len = end - base_port;

	if (hostname_len > sizeof(hostname) - 1) {
		fprintf(stderr, "Max hostname is %lu\n", sizeof(hostname) - 1);
		err = -1;
	}

	if (port_len > 5) {
		fprintf(stderr, "Max port number is 65535\n");
		err = -1;
	}

	if (err == -1) {
		return err;
	}

	strcpy(hostname, base_hostname);
	server_port = atoi(base_port);

	if (server_port < 1024 || server_port > 65535) {
		fprintf(stderr, "Provide correct port from range 1025-65535\n");
		err = -1;
	}

	return err;
}


/* Get random integer value from range (min,max) */
static unsigned int get_random(unsigned int min, unsigned int max)
{
	assert(max >= min);

	unsigned int range = max - min + 1;
	unsigned int random_value = rand() % range + min;

	return random_value;
}


int save_results(const char *log_dir)
{
	FILE *log;
	time_t curr_time;
	struct tm *curr_date;
	char path[256];

	time(&curr_time);
	curr_date = localtime(&curr_time);
	if (curr_date == NULL) {
		perror("localtime");
		return -1;
	}

	sprintf(log_file, "logs-%02d%02d%02d-%02d:%02d:%02d",
                      curr_date->tm_year - 100,
                      curr_date->tm_mon + 1,
                      curr_date->tm_mday,
                      curr_date->tm_hour,
                      curr_date->tm_min,
                      curr_date->tm_sec);

	sprintf(path, "%s/%s", log_dir, log_file);

	if ((log = fopen(path, "w")) == NULL) {
		fprintf(stderr, "Can't create log file in: %s\n", path);
		return -1;
	}

	fprintf(log, "Server Hostname: %s\n", hostname);
	fprintf(log, "Server Port: %d\n\n", server_port);
	fprintf(log, "Concurrent connections: %u\n", client_num);
	fprintf(log, "Total requests: %u\n", request_num);
	fprintf(log, "Completed requests: %u\n", completed_reqs);
	fprintf(log, "Failed requests: %u\n", failed_reqs);

	fclose(log);
	return 0;
}


void *client_thread(void *arg)
{
	client_t *client = (client_t *)arg;
	struct sockaddr_in server_addr, client_addr;
	int sockfd;
	int try = 10;
	const char *error_msg;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0) {
		error_msg = "Socket creation failed";
		pthread_exit((void *)error_msg);
	}

	/* Bind to user supplied interface name (must have IP assigned) */
	if (strlen(ifname) != 0 && strlen(ifaddr) != 0) {
		memset(&client_addr, 0, sizeof client_addr);
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = htons(0);
		inet_pton(AF_INET, ifaddr, &client_addr.sin_addr);

		if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof client_addr) < 0) {
			perror("bind");
			close(sockfd);
			exit(EXIT_FAILURE);
		}
	}

	memset(&server_addr, 0, sizeof server_addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(hostname);
	server_addr.sin_port = htons(server_port);

	for (int i = 0; i < client->req_num; i++) {
		const char *request;
		size_t req_len;
		unsigned int files_num = 0u;
		unsigned int idx;

		for (int i = 0; client->files[i] != NULL && i < MAX_FILES; i++) {
			files_num++;
		}

		while (connect(sockfd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
			if (try-- > 0) {
				close(sockfd);
				sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (sockfd < 0) {
					error_msg = "Socket creation failed";
					pthread_exit((void *)error_msg);
				}
				sleep(1);
			}
			else {
				close(sockfd);
				error_msg = "Couldn't connect to server";
				pthread_exit((void *)error_msg);
			}
		}

		/* Index of random file from client predefined files */
		idx = get_random(0, files_num - 1);

		printf("idx: %u\n", idx);

		request = create_request("GET", client->files[idx], request_header, NULL);
		if (request == NULL) {
			error_msg = "Request creation failed";
			pthread_exit((void *)error_msg);
		}
		req_len = strlen(request);

		error_msg = sendall(sockfd, request, req_len, MSG_NOSIGNAL);
		if (error_msg != NULL) {
			pthread_exit((void *)error_msg);
		}
		free((void *)request);

		// read

		usleep(client->interv * 1000);
	}

	return (void *)NULL;
}


typedef struct {
	int *fds;
	int *readyfds;
} thrargs_t;

void preperrmsg(char *prefix, char *buf, size_t buflen)
{
	size_t prefix_len;

	memset(buf, 0, buflen);

	strncpy(buf, prefix, buflen);
	buf[buflen - 1] = '\0';

	prefix_len = strlen(buf);

	if (prefix_len + 2 < buflen) {
		buf[prefix_len] = ':';
		buf[prefix_len + 1] = ' ';
		prefix_len += 2;
		buf[prefix_len] = '\0';
	}

	strerror_r(errno, buf + prefix_len, buflen - prefix_len);
}

void *polling_thread(void *arg)
{
	struct epoll_event ev, events[MAX_EXENTS];
	int nfds, epollfd;
	size_t emsg_len = 256;
	char *error_msg = NULL;
	thrargs_t *args = (thrargs_t *)arg;

	error_msg = (char *)malloc(emsg_len);
	if (error_msg == NULL) {
		pthread_exit("Couldn't allocate memory for error message\n");
	}

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		close(epollfd);
		preperrmsg("epoll_create1", error_msg, emsg_len);
		pthread_exit(error_msg);
	}

	for (int i = 0; i < client_num; i++) {
		ev.events = EPOLLIN;
		ev.data.fd = args->fds[i];
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, args->fds[i], &ev) == -1) {
			close(epollfd);
			preperrmsg("epoll_ctl", error_msg, emsg_len);
			pthread_exit(error_msg);
		}
	}

	for (;;) {
		nfds = epoll_wait(epollfd, events, MAX_EXENTS, -1);
		if (nfds == -1) {
			if (errno == EINTR) {
				/* No more events and signal prompting thread to exit was received */
				break;
			} else {
				close(epollfd);
				preperrmsg("epoll_wait", error_msg, emsg_len);
				pthread_exit(error_msg);
			}
		}

		for (int n = 0; n < nfds; n++) {
			if () {
				
			}
		}
	}

	return NULL;
}


int main(int argc, char **argv)
{
	int opt;
	int ret;

	while ((opt = getopt(argc, argv, ":n:c:i:vqh")) != -1) {
		switch(opt) {
			case 'n':
				request_num = atoi(optarg);
				if (client_num > MAX_REQS) {
					fprintf(stderr, "Maximum number of requests is: %u\n", MAX_REQS);
					exit(EXIT_FAILURE);
				}
				break;
			case 'c':
				client_num = atoi(optarg);
				if (client_num > MAX_CLIENTS) {
					fprintf(stderr, "Maximum number of client is: %u\n", MAX_CLIENTS);
					exit(EXIT_FAILURE);
				}
				break;
			case 'i':
				strncpy(ifname, optarg, IFNAMSIZ - 1);
				ifname[IFNAMSIZ - 1] = '\0';
				if (getifaddr(ifname, ifaddr) < 0) {
					exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				verbosity++;
				break;
			case 'q':
				is_quiet = 1;
				break;
			case 'h':
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case ':':
				fprintf(stderr, "Option -%c requires an operand\n", optopt);
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			case '?':
				fprintf(stderr,"Unrecognized option: -%c\n", optopt);
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
			default:
				/* NEVER GET HERE */
				fprintf(stderr, "getopt() unexpected behaviour\n");
				exit(EXIT_FAILURE);
		}
	}

	printf("ifname: %s\n", ifname);
	printf("ifaddr: %s\n", ifaddr);

	ret = parse_addr(argv[optind]);
	if (argv[optind] == NULL || ret == -2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	else if (ret == -1) {
		exit(EXIT_FAILURE);
	}

	/* Start connections */
	pthread_t *threads = (pthread_t *)malloc(client_num * sizeof(pthread_t));
	if (threads == NULL) {
		fprintf(stderr, "Not enough memory\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < client_num; i++) {
		ret = pthread_create(&threads[i], NULL, client_thread, (void*)&default_client);
		if (ret != 0) {
			fprintf(stderr, "Error creating thread number %d. Error code: %d\n", i, ret);
			exit(EXIT_FAILURE);
		}
	}

	/* Wait for clients to complete execution */
	for (int i = 0; i < client_num; i++) {
		void *thread_ret;

		ret = pthread_join(threads[i], &thread_ret);
		if (ret != 0) {
			fprintf(stderr, "Error while joining one of threads. Error code: %d\n", ret);
			exit(EXIT_FAILURE);
		}
		
		if (thread_ret != 0) {
			char *error_msg = (char *)thread_ret;
			fprintf(stderr, "One of threads finished unsuccessfully\n");
			fprintf(stderr, "Error message: %s\n", error_msg);
			exit(EXIT_FAILURE);
		}
	}
	
	/* Log results */
	if (is_quiet == 0) {
		ret = save_results("logs");
		if (ret < 0) {
			fprintf(stderr, "Error while saving results\n");
			exit(EXIT_FAILURE);
		}
	}

	printf("Clients finished execution.\n");
	if (is_quiet == 0) {
		printf("Results saved in %s.\n", log_file);
	}

	return EXIT_SUCCESS;
}
