#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// This is for setting websocket frame lengths that use more than one byte
union websocket_frame_length{
	uint64_t length;
	unsigned char bytes[8];
};

struct thread_pool {
	pthread_t thread;
	pthread_mutex_t mutex;
	struct thread_pool *next;
	int fd_index;
};

static const char* b64Table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Mostly config.txt options
int listener_socket;
unsigned char guest_suffix_min = 8,
	guest_suffix_max = 16,
	name_limit = 22,
	ping_pong_heartbeat_seconds = 45,
	ping_pong_response_seconds = 5,
	room_suffix_min = 8,
	room_suffix_max = 36,
	room_name_limit = 40,
	thread_count = 1;

unsigned short ping_pong_heartbeat_milliseconds = 0, ping_pong_response_milliseconds = 0, port = 8080;
unsigned int name_seed;
uint64_t max_msg_length = 1000;

int fd_counter = 1, fds_length = 4096;

// Where the poll socket data is stored
struct pollfd* fds;

pthread_mutex_t fds_mutex, sockets_root_mutex, rooms_root_mutex, thread_setting_socket_mutex;
pthread_t hk, sig;

#include "bst.c"
#include "functions.c"
#include "threads.c"

// Initialize the threads, and then just kind of do nothing...
int main(){
	// Verify that there is a config file... I can auto-create one later
	FILE *config = fopen("config.txt", "r+");
	if(config == NULL)
		printf("No config.txt found... using all defaults\n");

	else{
		// Read and parse the config file... Pretty basic right now
		char *line = NULL, *value;
		size_t line_len = 0;
		while(getline(&line, &line_len, config) != -1){
			value = strstr(line, ":");
			if(value == NULL)
				goto config_read_no_colon;

			value[0] = 0;
			value++;

			while(value[0] == ' ' || value[0] == '\t')
				value++;

			if(!strcmp(line, "max_message_length"))
				max_msg_length = (uint64_t)strtoull(value, NULL, 10);

			else if(!strcmp(line, "guest_name_random_min"))
				guest_suffix_min = (unsigned char)atoi(value);

			else if(!strcmp(line, "guest_name_random_max"))
				guest_suffix_max = (unsigned char)atoi(value);

			else if(!strcmp(line, "room_name_random_min"))
				room_suffix_min = (unsigned char)atoi(value);

			else if(!strcmp(line, "room_name_random_max"))
				room_suffix_max = (unsigned char)atoi(value);

			else if(!strcmp(line, "name_length_limit"))
				name_limit = (unsigned char)atoi(value);

			else if(!strcmp(line, "room_name_length_limit"))
				room_name_limit = (unsigned char)atoi(value);

			else if(!strcmp(line, "port"))
				port = (unsigned short)atoi(value);

			else if(!strcmp(line, "max_sockets"))
				fds_length = (uint64_t)strtoull(value, NULL, 10);

			else if(!strcmp(line, "threads"))
				thread_count = (unsigned char)atoi(value);

			else if(!strcmp(line, "ping_pong_heartbeat_seconds"))
				ping_pong_heartbeat_seconds = (unsigned short)atoi(value);

			else if(!strcmp(line, "ping_pong_heartbeat_milliseconds"))
				ping_pong_heartbeat_milliseconds = (unsigned short)atoi(value);

			else if(!strcmp(line, "ping_pong_response_seconds"))
				ping_pong_response_seconds = (unsigned short)atoi(value);

			else if(!strcmp(line, "ping_pong_response_milliseconds"))
				ping_pong_response_milliseconds = (unsigned short)atoi(value);

			else{
				config_read_no_colon:
				if(strcmp(line, "") && strcmp(line, "\n") && strncmp(line, ";", 1)){
					printf("Unknown configuration: \"%s\"!\n", line);
					return 0;
				}
			}

			free(line);
			line = NULL;
		}

		fclose(config);
	}

	fds = calloc(fds_length, sizeof(struct pollfd));

	//Set up the listener socket
	unsigned char val = 1;
	struct sockaddr_in listener_socket_sockaddr = {0};
	struct timespec timespec_seed;

	listener_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(listener_socket < 0 || setsockopt(listener_socket, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), &val, sizeof(int)) < 0){
		printf("Socket creation failed\n");
		exit(1);
	}

	listener_socket_sockaddr.sin_family = AF_INET;
	listener_socket_sockaddr.sin_port = htons(port);
	listener_socket_sockaddr.sin_addr.s_addr = INADDR_ANY;
	if(bind(listener_socket, (struct sockaddr *)&listener_socket_sockaddr, sizeof(listener_socket_sockaddr)) < 0){
		printf("Error binding the socket\n");
		exit(1);
	}
	listen(listener_socket, 4);

	fcntl(listener_socket, F_SETFL, fcntl(listener_socket, F_GETFL) | O_ASYNC);

	fds[0].fd = listener_socket;
	fds[0].events = POLLIN;

	pthread_mutex_init(&fds_mutex, NULL);
	pthread_mutex_init(&sockets_root_mutex, NULL);
	pthread_mutex_init(&rooms_root_mutex, NULL);
	pthread_mutex_init(&thread_setting_socket_mutex, NULL);
		
	// Set the clock and the name seed
	clock_gettime(CLOCK_REALTIME, &timespec_seed);

	name_seed = ((unsigned int)clock() + (unsigned int)timespec_seed.tv_nsec) * 256 * rand();
	srand(name_seed);

	// Now we set up SIGIO for the signal thread...
	fcntl(listener_socket, F_SETOWN, getpid());
	sigset_t signal_set;

	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGIO);

	// ...now we set the signal mask for the signal thread...
	if(pthread_sigmask(SIG_SETMASK, &signal_set, NULL) != 0){
		printf("Unable to set thread signal handling!\n");
		exit(1);
	}

	// ...and allocate memory for the threadpool early so we can pass the pointer to the signal thread...
	struct thread_pool *thread = malloc(sizeof(struct thread_pool)), *first_thread;
	first_thread = thread;

	// ...and then actually create the signal handling thread...
	if(pthread_create(&sig, NULL, sigFunc, first_thread) != 0){
		printf("Unable to create signal thread!\n");
		exit(1);
	}

	if(pthread_detach(sig) != 0){
		printf("Failed to detach signal thread...?\n");
		exit(1);
	}

	// ...then set up the threadpool...
	sigfillset(&signal_set);
	sigaddset(&signal_set, SIGUSR1);

	for(val=1;;){
		pthread_mutex_init(&(thread->mutex), NULL);

		thread->fd_index = -1;

		if(pthread_sigmask(SIG_SETMASK, &signal_set, NULL) != 0){
			printf("Unable to set thread signal handling for thread pool!\n");
			exit(1);
		}

		if(pthread_create(&(thread->thread), NULL, poolFunc, thread) != 0){
			printf("Creation of thread pool failed!\n");
			exit(1);
		}
	
		if(pthread_detach(thread->thread) != 0){
			printf("Failed to detach thread in pool...?\n");
			exit(1);
		}

		if(val++ < thread_count){
			thread->next = malloc(sizeof(struct thread_pool));
			thread = thread->next;
		}else{
			thread->next = first_thread;
			break;
		}
	}

	// ...and then end with making the house keeping thread.
	if(pthread_create(&hk, NULL, hkFunc, NULL) != 0){
		printf("Unable to create house keeping thread!\n");
		exit(1);
	}

	if(pthread_detach(hk) != 0){
		printf("Failed to detach house keeping thread...?\n");
		exit(1);
	}

	// Now drop to nobody privileges! Should make own user in perfect world, but I can deal with that later...
	if(setgid(65534) != 0){
		printf("Unable to drop group privileges!\n");
		exit(1);
	}

	if(setuid(65534) != 0){
		printf("Unable to drop user privileges!\n");
		exit(1);
	}

	// And we're done... Should probably remove all printfs for production...
	printf("Web socket server is listening...\n");
	// Also a special note, printf is technically not thread safe lmao. If you know a thread safe way to print debug info, let me know

	for(;;)
		pause();

	return 0;
}
