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


#define MAX_CLIENTS 10000u
#define MAX_REQS 1000u
#define MAX_FILES 10u

typedef struct client {
	char *files[MAX_FILES]; /* Paths to files client will request */
	unsigned int req_num;   /* Number of request client will make */
	int interv;             /* Time between requests in ms, -1 if random */
	int max_latency;        /* Maximum acceptable latency of data retrival in ms, -1 if no latency check */
} client_t;

/* ------------------------ GLOBALS ------------------------ */

static unsigned int verbosity = 0;     /* No verbosity on default */
static unsigned int is_quiet = 0;       /* Quiet run flag */
static const client_t default_client = {
	.files = {"/"},
	.req_num = 10,
	.interv = 500,
	.max_latency = -1
}; /* Default client type */
static unsigned int client_num = 10;   /* Number of concurrent clients */
static unsigned int request_num = 100; /* Number of all requests */
static char *if_name;                  /* Name of network interface */
static char hostname[33];              /* Name of server */
static unsigned int port = 80;         /* Server port */
static const char *config_path;        /* Path to config */

/* ------------------------ REQUESTS ----------------------- */

const char *request = "GET / HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "User-Agent: SimpleHTTPClient\r\n"
                      "Connection: close\r\n\r\n";


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
		return -2;
	}

	if_flags = ifr.ifr_flags;
	if (if_flags & IFF_RUNNING) {
		return 0;
	}

	return -1;
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
	port = atoi(base_port);

	if (port < 1024 || port > 65535) {
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


void *client_thread(void *arg)
{

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
				ret = check_valid_interface(if_name);
				if (ret == -1) {
					fprintf(stderr, "Interface '%s' is not running\n", if_name);
					exit(EXIT_FAILURE);
				} else if (ret == -2) {
					fprintf(stderr, "No such interface: %s\n", if_name);
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
	} else if (ret == -1) {
		exit(EXIT_FAILURE);
	}

	printf("hostname: %s\nport: %hu\n", hostname, port);
	
	// read config

	// start clients

	// log data

	// join clients

	return EXIT_SUCCESS;
}
