// This thread should handle PING/PONG and maybe other stuff
static void* hkFunc(){
//	struct timespec ping_pong_heartbeat_tv, ping_pong_response_tv;
//	pingong_heartbeat_tv.tv_sec = pingPongHeartbeatSeconds;
//	ping_pong_heartbeat_tv.tv_nsec = pingPongHeartbeatMilliseconds * 1000000;

//	ping_pong_response_tv.tv_sec = pingPongResponseSeconds;
//	ping_pong_response_tv.tv_nsec = pingPongResponseMilliseconds * 1000000;

	for(;;)
		pause();

	return NULL;
}

// A recursive function used in the signal handler to find the active socket
int findActiveSocket(int offset, int length, struct threadPool *thread){
	if(poll(&pollFDs[offset], length, 0) < 1)
		return -1;

	if(length == 1){
		struct threadPool *tmp_thread = thread;
		do{
			if(tmp_thread->fd_index == offset)
				return -1;

			tmp_thread = tmp_thread->next;
		}while(tmp_thread != thread);

		return offset;
	}

	int half = length >> 1, ret = findActiveSocket(offset, half, thread);
	if(ret == -1)
		return findActiveSocket(offset + half, length - half, thread);

	return ret;
}

// Signal thread. Used to catch SIGIO, find the active socket, and then pass it on to the first avialballe thread in the threadpool
static void* sigFunc(void *arg){
	struct threadPool *thread = (struct threadPool*)arg;
	sigset_t signal_set;
	int caught_signal, fd_index;

	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGIO);

	for(;;){
		sigwait(&signal_set, &caught_signal);

		if(caught_signal != SIGIO){
			if(caught_signal != 0)
				printf("Signal not SIGIO? %i\n", caught_signal);

			continue;
		}

		// In my opinion, this is mor readbale using goto than a loop
		sig_func_do_loop:
		pthread_mutex_lock(&pollFDsMutex);
		fd_index = findActiveSocket(0, pollFDsLength, thread);
		pthread_mutex_unlock(&pollFDsMutex);

		if(fd_index != -1){
			printf("Found activity on fd_index %i\n", fd_index);

			while(pthread_mutex_trylock(&(thread->mutex)) != 0)
				thread = thread->next;

			thread->fd_index = fd_index;
			pthread_kill(thread->thread, SIGUSR1);
			thread = thread->next;
			goto sig_func_do_loop;
		}
	}

	return NULL;
}

// The threadpool function. This will run in parallel with itself. This is the "heavy lifting" for when a socket has activity
static void* poolFunc(void *arg){
	char buffer[4096] = {0};
	int caught_signal;

	struct threadPool *this = arg;
	sigset_t signal_set;

	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGUSR1);

	// When the signal handler has set everything up, it will send signal SIGUSR1
	for(;;){
		sigwait(&signal_set, &caught_signal);

		if(caught_signal != SIGUSR1){
			if(caught_signal != 0)
				printf("Threadpool - wrong signal? %i\n", caught_signal);
			else
				continue;

			goto threadpool_done_unlock;
		}

		printf("\nThread - active fd_index %i!\n", this->fd_index);

		// If the fd_index is 0, it's the listener socket that has activity, which means there's a new socket attempting to open a connection
		if(!this->fd_index){
			int new_socket = accept(pollFDs[0].fd, NULL, NULL);
			if(new_socket == -1)
				goto threadpool_done_unlock;

			fcntl(new_socket, F_SETFL, fcntl(new_socket, F_GETFL) | O_ASYNC);
			fcntl(new_socket, F_SETOWN, getpid());

			recv(new_socket, buffer, 4095, 0);

			// Adding the socket to pollFDs
			pthread_mutex_lock(&pollFDsMutex);

			this->fd_index = fdCounter;

			// Setting fd_counteir to the next open fd_index
			while(pollFDs[++fdCounter].fd != 0){
				if(fdCounter >= pollFDsLength){
					printf("TOO MANY SOCKETS! Exiting now...\n");
					exit(1);
				}
			}

			pollFDs[this->fd_index].fd = new_socket;
			pollFDs[this->fd_index].events = POLLIN;

			pthread_mutex_unlock(&pollFDsMutex);

			// Now we need to read the buffer and do websocket stuff according to https://tools.ietf.org/html/rfc6455#section-1.3
			char *wskey = strstr(buffer, "\r\nSec-WebSocket-Key: ");
			if(wskey == NULL){
				shutdown(new_socket, SHUT_RDWR);
				close(new_socket);
				goto threadpool_done_unlock;
			}
			wskey[45] = 0;

			char tmp[61] = {0};
			sprintf(tmp, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", &wskey[21]);

			// Now we hash the tmp buffer with SHA1
			SHA1((const unsigned char*)tmp, 60, (unsigned char*)tmp);
			strcpy(buffer, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ");

			// And base64_encode it
			buffer[97] = b64Table[(unsigned char)tmp[0]>>2];
			buffer[98] = b64Table[((0x3&(unsigned char)tmp[0])<<4) + ((unsigned char)tmp[1]>>4)];
			buffer[99] = b64Table[((0x0f&(unsigned char)tmp[1])<<2) + ((unsigned char)tmp[2]>>6)];
			buffer[100] = b64Table[0x3f&(unsigned char)tmp[2]];

			buffer[101] = b64Table[(unsigned char)tmp[3]>>2];
			buffer[102] = b64Table[((0x3&(unsigned char)tmp[3])<<4) + ((unsigned char)tmp[4]>>4)];
			buffer[103] = b64Table[((0x0f&(unsigned char)tmp[4])<<2) + ((unsigned char)tmp[5]>>6)];
			buffer[104] = b64Table[0x3f&(unsigned char)tmp[5]];

			buffer[105] = b64Table[(unsigned char)tmp[6]>>2];
			buffer[106] = b64Table[((0x3&(unsigned char)tmp[6])<<4) + ((unsigned char)tmp[7]>>4)];
			buffer[107] = b64Table[((0x0f&(unsigned char)tmp[7])<<2) + ((unsigned char)tmp[8]>>6)];
			buffer[108] = b64Table[0x3f&(unsigned char)tmp[8]];

			buffer[109] = b64Table[(unsigned char)tmp[9]>>2];
			buffer[110] = b64Table[((0x3&(unsigned char)tmp[9])<<4) + ((unsigned char)tmp[10]>>4)];
			buffer[111] = b64Table[((0x0f&(unsigned char)tmp[10])<<2) + ((unsigned char)tmp[11]>>6)];
			buffer[112] = b64Table[0x3f&(unsigned char)tmp[11]];

			buffer[113] = b64Table[(unsigned char)tmp[12]>>2];
			buffer[114] = b64Table[((0x3&(unsigned char)tmp[12])<<4) + ((unsigned char)tmp[13]>>4)];
			buffer[115] = b64Table[((0x0f&(unsigned char)tmp[13])<<2) + ((unsigned char)tmp[14]>>6)];
			buffer[116] = b64Table[0x3f&(unsigned char)tmp[14]];

			buffer[117] = b64Table[(unsigned char)tmp[15]>>2];
			buffer[118] = b64Table[((0x3&(unsigned char)tmp[15])<<4) + ((unsigned char)tmp[16]>>4)];
			buffer[119] = b64Table[((0x0f&(unsigned char)tmp[16])<<2) + ((unsigned char)tmp[17]>>6)];
			buffer[120] = b64Table[0x3f&(unsigned char)tmp[17]];

			buffer[121] = b64Table[(unsigned char)tmp[18]>>2];
			buffer[122] = b64Table[((0x3&(unsigned char)tmp[18])<<4) + ((unsigned char)tmp[19]>>4)];
			buffer[123] = b64Table[((0x0f&(unsigned char)tmp[19])<<2)];

			strcpy(&buffer[124], "=\r\n\r\n");

			if(strlen(buffer) == 0 || send(new_socket, buffer, 129, MSG_NOSIGNAL) < 1){
				shutdown(new_socket, SHUT_RDWR);
				close(new_socket);
				continue;
			}

			
			// Insert socket "BST" node
			struct socketBST* socket_node = malloc(sizeof(struct socketBST));

			sprintf(buffer, "%i", this->fd_index);
			socket_node->index_length = strlen(buffer) + 1;
			socket_node->index = malloc(socket_node->index_length);
			strcpy(socket_node->index, buffer);

			socket_node->fd = new_socket;

			pthread_mutex_init(&socket_node->identities_mutex, NULL);
			socket_node->identities = NULL;

			pthread_mutex_lock(&socketsRootMutex);
			bstInsert(socket_node, (void**)&socketsRoot);
			pthread_mutex_unlock(&socketsRootMutex);

			goto threadpool_done_unlock;
		}

		// Else, if it's not the listener socket that has activity, we handle it here: https://tools.ietf.org/html/rfc6455#section-5
		while(poll(&pollFDs[this->fd_index], 1, 0)){
			char mask[4];
			union websocketFrameLength len;

			ssize_t this_read_length, read_length = 0;
			while(read_length < 2)
				if((this_read_length = recv(pollFDs[this->fd_index].fd, &buffer[read_length], 2 - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			printf("First 2: %hhu %hhu\n", (unsigned char)buffer[0], (unsigned char)buffer[1]);

			//   FIN & op = 1          Mask set
			if(buffer[0] != -127 || buffer[1] > -1){
				// If ths socket is closed, it might be handled here? Test this please
				while(recv(pollFDs[this->fd_index].fd, buffer, 4096, 0) == 4096){}
				close_socket(this->fd_index);
				goto threadpool_done_unlock;
			}

			len.length = ((unsigned char)buffer[1]) % 128;
			printf("First length: %"PRIu64"\n", len.length);
			if(len.length == 127){
				read_length = 0;
				while(read_length < 8)
					if((this_read_length = recv(pollFDs[this->fd_index].fd, &buffer[read_length], 8 - read_length, 0)) > 0)
						read_length += this_read_length;
					else{
						close_socket(this->fd_index);
						goto threadpool_done_unlock;
					}

				len.bytes[0] = (unsigned char)buffer[7];
				len.bytes[1] = (unsigned char)buffer[6];
				len.bytes[2] = (unsigned char)buffer[5];
				len.bytes[3] = (unsigned char)buffer[4];
				len.bytes[4] = (unsigned char)buffer[3];
				len.bytes[5] = (unsigned char)buffer[2];
				len.bytes[6] = (unsigned char)buffer[1];
				len.bytes[7] = (unsigned char)buffer[0];
			}if(len.length == 126){
				read_length = 0;
				while(read_length < 2)
					if((this_read_length = recv(pollFDs[this->fd_index].fd, &buffer[read_length], 2 - read_length, 0)) > 0)
						read_length += this_read_length;
					else{
						close_socket(this->fd_index);
						goto threadpool_done_unlock;
					}

				len.bytes[0] = (unsigned char)buffer[1];
				len.bytes[1] = (unsigned char)buffer[0];
			}

			printf("Final length: %"PRIu64"\n", len.length);

			read_length = 0;
			while(read_length < 4)
				if((this_read_length = recv(pollFDs[this->fd_index].fd, &mask[read_length], 4 - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			// I'd like to set this up to read from the socket in chunks rather than malloc... eventually. Or maybe do some magical socket stuff I don't know how to do yet.
			char *msg = malloc(len.length + 1);

			read_length = 0;
			while((uint64_t)read_length < len.length)
				if((this_read_length = recv(pollFDs[this->fd_index].fd, &msg[read_length], len.length - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			msg[read_length] = 0;

			while(read_length-- > -1)
				msg[read_length] ^= mask[read_length%4];

			// Sometimes the Tor Browser Bundle will randomly send "(null)"? I just skip these so that the browser doesn't spam the chat
			if(!strcmp("(null)", msg))
				goto threadpool_done_unlock;

			// If/Else ladder of commands
			else if(!strcmp("chat", msg)){
				// If it's a normal chat message...
				char *message, *prepared_message;

				message = strchr(&msg[5], 0) + 1;
				message = strchr(message, 0) + 1;

				stringChop(&message, maxMsgLength);

				sprintf(buffer, "%i", this->fd_index);
				pthread_mutex_lock(&socketsRootMutex);
				struct socketBST *s_node = bstSearch(buffer, strlen(buffer)+1, socketsRoot);
				pthread_mutex_unlock(&socketsRootMutex);
				pthread_mutex_lock(&s_node->identities_mutex);
				struct socketIdentityBST *s_identity = bstSearch((void*)&msg[5], message - &msg[5], s_node->identities);
				pthread_mutex_unlock(&s_node->identities_mutex);

				if(s_identity == NULL){
					sendToSocket("error\0Invalid identity", 22, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				prepared_message = malloc(strlen(message) + strlen(s_identity->identity->name) + 28);
				sprintf(prepared_message, "chat%c%s%c%s%c%s", 0, s_identity->identity->name, 0, s_identity->identity->trip, 0, message);
				sendToRoom(prepared_message, strlen(message) + strlen(s_identity->identity->name) + 27, s_identity->identity->room_node);
				free(prepared_message);

			}else if(!strcmp("join", msg)){
				// If it's a user joining a new room
				char *room = &msg[5], *name, *tmp;

				if(len.length < 7 || 6 + strlen(room) == len.length){
					sendToSocket("error\0Invalid join!", 19, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				name = &room[strlen(room) + 1];

				if(!strlen(name)){
					pthread_mutex_lock(&randMutex);
					unsigned char tmplen = rand() % (guestSuffixMax - guestSuffixMin);
					pthread_mutex_unlock(&randMutex);
					name = malloc(6 + guestSuffixMin + tmplen);
					strcpy(name, "Guest");
					randomSuffix(&name, guestSuffixMin + tmplen);
				}else{
					stringChop(&name, nameLimit);
					tmp = malloc(strlen(name) + 1);
					strcpy(tmp, name);
					name = tmp;
				}

				if(!strlen(room)){
					pthread_mutex_lock(&randMutex);
					unsigned char tmplen = rand() % (roomSuffixMax - roomSuffixMin);
					pthread_mutex_unlock(&randMutex);
					room = malloc(5 + roomSuffixMin + tmplen);
					strcpy(room, "Room");
					randomSuffix(&room, roomSuffixMin + tmplen);
				}else{
					stringChop(&room, roomNameLimit);
					tmp = malloc(strlen(room) + 1);
					strcpy(tmp, room);
					room = tmp;
				}

				pthread_mutex_lock(&roomsRootMutex);
				struct roomBST *r_node = bstSearch(room, strlen(room) + 1, roomsRoot);
				pthread_mutex_unlock(&roomsRootMutex);

				if(r_node == NULL){
					r_node = malloc(sizeof(struct roomBST));
					r_node->index_length = strlen(room)+1;
					r_node->index = malloc(r_node->index_length);
					strcpy(r_node->index, room);

					pthread_mutex_init(&r_node->identities_mutex, NULL);
					r_node->identities = NULL;
					r_node->first = NULL;
					r_node->last = NULL;

					// previously rooms_root_mutex was also sent to the function
					pthread_mutex_lock(&roomsRootMutex);
					bstInsert(r_node, (void**)&roomsRoot);
					pthread_mutex_unlock(&roomsRootMutex);
				}else{
					// Make sure the name isn't already being used in that room...
					sprintf(buffer, "%s%c%s", room, 0, name);

					pthread_mutex_lock(&r_node->identities_mutex);
					struct roomIdentityBST* tmp = bstSearch(buffer, strlen(name)+strlen(room)+2, r_node->identities);
					pthread_mutex_unlock(&r_node->identities_mutex);

					if(tmp != NULL){
						sendToSocket("error\0This username is already taken!", 37, pollFDs[this->fd_index].fd);

						free(room);
						free(name);
						goto threadpool_done_unlock;
					}
				}

				struct socketIdentityBST *s_identity = malloc(sizeof(struct socketIdentityBST));
				s_identity->identity = malloc(sizeof(struct identityNode));

				// Generate name color
				unsigned int seed = colorSeed;
				unsigned short i;

				for(i=0; i<strlen(name); i++)
					seed *= (unsigned char)name[i] - i;

				printf("%u\n", seed);

				pthread_mutex_lock(&randMutex);
				srand(seed);
				s_identity->identity->color = malloc(7);
				sprintf(s_identity->identity->color, "%X", rand()%191 + 64 + (rand()%191 + 64) * 0x100 + (rand()%191 + 64) * 0x10000);

				struct timespec timespecSeed;
				clock_gettime(CLOCK_REALTIME, &timespecSeed);

				srand(((unsigned int)clock() + (unsigned int)timespecSeed.tv_nsec) * colorSeed);
				pthread_mutex_unlock(&randMutex);

				s_identity->index_length = strlen(room) + strlen(name) + 2;
				s_identity->index = malloc(s_identity->index_length);
				sprintf(s_identity->index, "%s%c%s", room, 0, name);

				// Let's just set up identityNode as an element of s_identity because it's already there
				s_identity->identity->name = malloc(strlen(name)+1);
				strcpy(s_identity->identity->name, name);
				s_identity->identity->trip = malloc(21);
				strcpy(s_identity->identity->trip, "abcdefghijklmnopqrst");

				sprintf(buffer, "%i", this->fd_index);
				pthread_mutex_lock(&socketsRootMutex);
				s_identity->identity->socket_node = bstSearch(buffer, strlen(buffer)+1, socketsRoot);
				pthread_mutex_unlock(&socketsRootMutex);
				s_identity->identity->room_node = r_node;

				struct roomIdentityBST *r_identity = malloc(sizeof(struct roomIdentityBST));
				memcpy(r_identity, s_identity, ((sizeof(char*) * 5) + sizeof(unsigned int) + sizeof(enum rbColors)));
				r_identity->next = NULL;

				// NOTE: currently sending the same tempIdentity to both sIdentity and rIdentity
				// When one or the other gets removed, tempIdentity will be freed and the pointer in the other will be broken
				// Consider making one malloced copy for each.

				if(r_node->identities == NULL){
					r_node->first = r_identity;
					r_node->last = r_node->first;
				}else{
					r_node->last->next = r_identity;
					r_node->last = r_node->last->next;
				}

				pthread_mutex_lock(&r_node->identities_mutex);
				bstInsert(r_identity, (void**)&r_node->identities);
				pthread_mutex_unlock(&r_node->identities_mutex);

				pthread_mutex_lock(&s_identity->identity->socket_node->identities_mutex);
				bstInsert(s_identity, (void**)&s_identity->identity->socket_node->identities);
				pthread_mutex_unlock(&s_identity->identity->socket_node->identities_mutex);

				sprintf(buffer, "joined%c%s%c%s%c%s", 0, room, 0, name, 0, s_identity->identity->trip);
				sendToSocket(buffer, 29 + strlen(name) + strlen(room), pollFDs[this->fd_index].fd);

				struct roomIdentityBST *users = r_node->first;
				while(users != NULL){
					sprintf(buffer, "hi%c%s%c%s%c%s", 0, users->identity->name, 0, users->identity->trip, 0, users->identity->color);
					memcpy(&buffer[26 + strlen(users->identity->name) + strlen(users->identity->color)], s_identity->index, s_identity->index_length);
					sendToSocket(buffer, 25 + strlen(users->identity->name) + strlen(users->identity->color) + s_identity->index_length, pollFDs[this->fd_index].fd);

					if(r_identity != users){
						sprintf(buffer, "hi%c%s%c%s%c%s%c%s", 0, s_identity->identity->name, 0, s_identity->identity->trip, 0, s_identity->identity->color, 0, users->identity->name);
						memcpy(&buffer[26 + strlen(s_identity->identity->name) + strlen(s_identity->identity->color)], users->index, users->index_length);
						sendToSocket(buffer, 25 + strlen(s_identity->identity->name) + strlen(s_identity->identity->color) + users->index_length, users->identity->socket_node->fd);
					}

					users = bstNext(users);
				}

				free(room);
				free(name);

			}else if(!strcmp("invite", msg)){
				unsigned char valid = 1;
				char *room = &msg[7], *name, *to_room;

				if(len.length < 9 || 8 + strlen(room) == len.length)
					valid = 0;

				if(valid && 9 + strlen(room) + strlen((name = &room[strlen(room) + 1])) == len.length)
					valid = 0;

				if(!valid){
					sendToSocket("error\0Invalid invite!", 21, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				to_room = &name[strlen(name) + 1];

				if(!strcmp(room, to_room)){
					sendToSocket("error\0Can't invite to current room!", 35, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				// We need to verify that the room the other user is being invited to is a room that this current user is in

				sprintf(buffer, "%i", this->fd_index);
				pthread_mutex_lock(&socketsRootMutex);
				struct socketBST* this_socket_node = bstSearch(buffer, strlen(buffer)+1, socketsRoot);
				pthread_mutex_unlock(&socketsRootMutex);

				pthread_mutex_lock(&this_socket_node->identities_mutex);
				struct socketIdentityBST* socket_identity_tmp = bstSearch(to_room, strlen(to_room)+1, this_socket_node->identities);
				pthread_mutex_unlock(&this_socket_node->identities_mutex);

				if(socket_identity_tmp == NULL){
					sendToSocket("error\0Invalid room!", 19, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}


				// Now we have to find the room/user they are trying to invite...
				pthread_mutex_lock(&roomsRootMutex);
				struct roomBST* room_node = bstSearch(room, strlen(room) + 1, roomsRoot);
				pthread_mutex_unlock(&roomsRootMutex);
				if(room_node == NULL){
					sendToSocket("error\0Room not found!", 21, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				pthread_mutex_lock(&room_node->identities_mutex);
				struct roomIdentityBST* identity_node = bstSearch(room, strlen(name) + strlen(room) + 2, room_node->identities);
				pthread_mutex_unlock(&room_node->identities_mutex);
				if(identity_node == NULL){
					sendToSocket("error\0User not found!", 21, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				// Then get the socket number and send the invite!
				sprintf(buffer, "invite%c%s", 0, to_room);

				sendToSocket(buffer, 7 + strlen(to_room), identity_node->identity->socket_node->fd);

			}else if(!strcmp("leave", msg)){
				unsigned short index_length = strlen(&msg[6]);

				if(len.length < 8 || (unsigned short)(7 + index_length) == len.length){
					sendToSocket("error\0Invalid leave!", 20, pollFDs[this->fd_index].fd);
					goto free_and_continue;
				}

				index_length += strlen(&msg[index_length + 1]) + 2;

				sprintf(buffer, "%i", this->fd_index);
				pthread_mutex_lock(&socketsRootMutex);
				struct socketBST* this_socket_node = bstSearch(buffer, strlen(buffer)+1, socketsRoot);
				pthread_mutex_unlock(&socketsRootMutex);

				pthread_mutex_lock(&this_socket_node->identities_mutex);
				struct socketIdentityBST *socket_identity = bstSearch(&msg[6], index_length, this_socket_node->identities);
				pthread_mutex_unlock(&this_socket_node->identities_mutex);

				struct identityNode *identity_node = this_socket_node->identities->identity;

				pthread_mutex_lock(&identity_node->room_node->identities_mutex);
				struct roomIdentityBST *room_identity = bstSearch(&msg[6], index_length, identity_node->room_node->identities);
				pthread_mutex_unlock(&identity_node->room_node->identities_mutex);

				if(room_identity == identity_node->room_node->first)
					identity_node->room_node->first = identity_node->room_node->first->next;
				else{
				}

				bstRemoveNode(socket_identity, (void**)&this_socket_node->identities);
				bstRemoveNode(room_identity, (void**)&identity_node->room_node->identities);

				sendToSocket(msg, len.length, pollFDs[this->fd_index].fd);

				char *buffer = malloc(6 + strlen(identity_node->name) + strlen(identity_node->trip));
				sprintf(buffer, "bye%c%s%c%s", 0, identity_node->name, 0, identity_node->trip);
				sendToRoom(buffer, 5 + strlen(identity_node->name) + strlen(identity_node->trip), identity_node->room_node);

				free(buffer);
				free(identity_node->name);
				free(identity_node->color);
				free(identity_node->trip);
				free(identity_node);
			}else
				// Else, probably someone trying to hack me
				sendToSocket("error\0Unrecognized input!", 25, pollFDs[this->fd_index].fd);

			free_and_continue:
			if(msg != NULL){
				free(msg);
				msg = NULL;
			}
		}

		// Set this thread's fd_index to -1 and then unlock the thread mutex
		threadpool_done_unlock:
		this->fd_index = -1;
		pthread_mutex_unlock(&(this->mutex));
	}

	return NULL;
}

