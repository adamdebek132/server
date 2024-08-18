/*
 * Http server benchmark
 *
 * Author: Adam Debek
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <common.h>


#define MAX_CLIENTS 10000u
#define MAX_REQS    1000u
#define MAX_FILES   10u

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
	.interv = 500,
	.max_latency = -1
}; /* Default client type */
static unsigned int client_num = 10;   /* Number of concurrent clients */
static unsigned int request_num = 100; /* Number of all requests */
static char *if_name;                  /* Name of network interface */
static char hostname[33];              /* Name of server */
static unsigned int server_port = 0;   /* Server port to connect to */
static char *config_path = NULL;       /* Path to config */
static char *log_file = NULL;          /* Log file name */

/* ------------------------ REQUESTS ----------------------- */

static const char *request_header = "User-Agent: AdamBenchmark\r\n"
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
	char *prog_name;
	prog_name = basename(prog_path);

	fprintf(stderr, "Usage: %s <hostname:port>\n", prog_name);
}


static int check_valid_interface(const char *if_name)
{
	struct ifreq ifr;
	short if_flags;
	int ret;
	int sockfd;

	if (if_name == NULL) {
		return -1;
	}

	strcpy(ifr.ifr_name, if_name);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	ret = ioctl(sockfd, SIOCGIFFLAGS, &ifr);
	if (ret < 0) {
		/* Unable to obtain flags */
		fprintf(stderr, "No such interface: %s\n", if_name);
		return -1;
	}

	if_flags = ifr.ifr_flags;
	if ((if_flags & IFF_RUNNING) == 0) {
		fprintf(stderr, "Interface '%s' is not running\n", if_name);
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


/* Get random value from range (min,max) */
static unsigned int get_random(unsigned int min, unsigned int max)
{
	unsigned int range = max - min + 1;
	unsigned int random_value = rand() % range + min;

	return random_value;
}


int parse_conf(const char *path)
{

	return 0;
}


int save_results(const char *path, char *log_file)
{

	return 0;
}


void *client_thread(void *arg)
{
	client_t *client = (client_t *)arg;
	struct sockaddr_in server_addr;
	int sockfd;
	int try = 10;
	const char *error_msg;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0) {
		error_msg = "Socket creation failed";
		pthread_exit((void *)error_msg);
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


int main(int argc, char **argv)
{
	int opt;
	int ret;

	while ((opt = getopt(argc, argv, ":n:c:f:i:vqh")) != -1) {
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
			case 'f':
				config_path = optarg;
				if (parse_conf(config_path) < 0) {
					fprintf(stderr, "Error while parsing config file '%s'\n", config_path);
					exit(EXIT_FAILURE);
				}
				break;
			case 'i':
				if_name = optarg;
				if (check_valid_interface(if_name) < 0) {
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

	ret = parse_addr(argv[optind]);
	if (argv[optind] == NULL || ret == -2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	else if (ret == -1) {
		exit(EXIT_FAILURE);
	}

	// read config
	if (config_path != NULL) {
		ret = parse_conf(config_path);
		if (ret < 0) {
			fprintf(stderr, "Error while parsing configure file\n");
			exit(EXIT_FAILURE);
		}
	}

	// start clients
	pthread_t threads[client_num];
	for (int i = 0; i < client_num; i++) {
		if (config_path == NULL) {
			ret = pthread_create(&threads[i], NULL, client_thread, (void*)&default_client);
			if (ret != 0) {
				fprintf(stderr, "Error creating thread number %d. Error code: %d\n", i, ret);
				exit(EXIT_FAILURE);
			}
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
	
	// log results
	if (is_quiet == 0) {
		ret = save_results("../logs", log_file);
		if (ret < 0) {
			fprintf(stderr, "Error while saving results\n");
			exit(EXIT_FAILURE);
		}
	}

	printf("Clients finished execution.\n");
	if (is_quiet == 0 && log_file != NULL) {
		printf("Results saved in %s.\n", log_file);
	}

	return EXIT_SUCCESS;
}
