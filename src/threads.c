static void* hkFunc(){
//	struct timespec ping_pong_heartbeat_tv, ping_pong_response_tv;
//	ping_pong_heartbeat_tv.tv_sec = ping_pong_heartbeat_seconds;
//	ping_pong_heartbeat_tv.tv_nsec = ping_pong_heartbeat_milliseconds * 1000000;

//	ping_pong_response_tv.tv_sec = ping_pong_response_seconds;
//	ping_pong_response_tv.tv_nsec = ping_pong_response_milliseconds * 1000000;

	for(;;)
		pause();

	// Rebalance tree
	// PING/PONG

	return NULL;
}

// A recursive function used in the signal handler
int findActiveSocket(int offset, int length, struct thread_pool *thread){
	if(poll(&fds[offset], length, 0) < 1)
		return -1;

	if(length == 1){
		struct thread_pool *tmp_thread = thread;
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

// Supposedly, locking mutexes can cause deadlocks in signal handling functions, but, since this is its own thread, I don't think it will happen
static void* sigFunc(void *arg){
	struct thread_pool *thread = (struct thread_pool*)arg;
	sigset_t signal_set;
	int caught_signal, fd_index;

	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGIO);

	for(;;){
		printf("Waiting...\n");
		sigwait(&signal_set, &caught_signal);
		printf("CAUGHT SIGIO!\n");

		if(caught_signal != SIGIO){
			if(caught_signal != 0)
				printf("Another signal? %i\n", caught_signal);

			continue;
		}

		// I think this looks better than a do-while with an if in the middle
		sig_func_do_loop:
		pthread_mutex_lock(&fds_mutex);
		fd_index = findActiveSocket(0, fds_length, thread);
		pthread_mutex_unlock(&fds_mutex);


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

static void* poolFunc(void *arg){
	char buffer[4096] = {0};
	int caught_signal;

	struct thread_pool *this = arg;
	sigset_t signal_set;

	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGUSR1);

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

		if(!this->fd_index){
			// Accepting the new socket
			int new_socket = accept(listener_socket, NULL, NULL);
			if(new_socket == -1)
				goto threadpool_done_unlock;

			fcntl(new_socket, F_SETFL, fcntl(new_socket, F_GETFL) | O_ASYNC);
			fcntl(new_socket, F_SETOWN, getpid());

			recv(new_socket, buffer, 4095, 0);

			pthread_mutex_lock(&fds_mutex);

			this->fd_index = fd_counter;

			while(fds[++fd_counter].fd != 0){
				if(fd_counter >= fds_length){
					printf("TOO MANY SOCKETS! Exiting now...\n");
					exit(1);
				}
			}

			// Adding this to fds without a mutex doesn't matter
			fds[this->fd_index].fd = new_socket;
			fds[this->fd_index].events = POLLIN;

			pthread_mutex_unlock(&fds_mutex);

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
			sprintf(buffer, "%i", this->fd_index);

			struct socketBST *socket_node = malloc(sizeof(struct socketBST));
			socket_node->name = malloc(strlen(buffer) + 1);
			strcpy(socket_node->name, buffer);

			socket_node->fd = new_socket;
			socket_node->rb = 0;
			socket_node->identities = NULL;
			socket_node->left = NULL;
			socket_node->right = NULL;

			pthread_mutex_init(&socket_node->identities_mutex, NULL);

			insertNode((void**)&sockets_root, (void*)socket_node, &sockets_root_mutex);

			goto threadpool_done_unlock;
		}

		while(poll(&fds[this->fd_index], 1, 0)){
			char mask[4];
			union websocket_frame_length len;

			ssize_t this_read_length, read_length = 0;
			while(read_length < 2)
				if((this_read_length = recv(fds[this->fd_index].fd, &buffer[read_length], 2 - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					printf("nope 1");
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			printf("First 2: %hhu %hhu\n", (unsigned char)buffer[0], (unsigned char)buffer[1]);

			//   FIN & op = 1          Mask set
			if(buffer[0] != -127 || buffer[1] > -1){
				printf("nope 2\n");
				while(recv(fds[this->fd_index].fd, buffer, 4096, 0) == 4096){}
				close_socket(this->fd_index);
				goto threadpool_done_unlock;
			}

			len.length = ((unsigned char)buffer[1]) % 128;
			printf("First length: %"PRIu64"\n", len.length);
			if(len.length == 127){
				read_length = 0;
				while(read_length < 8)
					if((this_read_length = recv(fds[this->fd_index].fd, &buffer[read_length], 8 - read_length, 0)) > 0)
						read_length += this_read_length;
					else{
						printf("nope 3");
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
					if((this_read_length = recv(fds[this->fd_index].fd, &buffer[read_length], 2 - read_length, 0)) > 0)
						read_length += this_read_length;
					else{
					printf("nope 4");
						close_socket(this->fd_index);
						goto threadpool_done_unlock;
					}

				len.bytes[0] = (unsigned char)buffer[1];
				len.bytes[1] = (unsigned char)buffer[0];
			}

			printf("Final length: %"PRIu64"\n", len.length);

			read_length = 0;
			while(read_length < 4)
				if((this_read_length = recv(fds[this->fd_index].fd, &mask[read_length], 4 - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					printf("nope 5");
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			// Preliminary check for message length before we allocate memory. We assume, worst case, everything is really long unicode
			if(len.length > max_msg_length * 4){
				while(recv(fds[this->fd_index].fd, buffer, 4096, 0) == 4096){}
				printf("Message too long with length: %"PRIu64"\n", len.length);
				goto threadpool_done_unlock;
			}

			char *msg = malloc(len.length);

			read_length = 0;
			while((uint64_t)read_length < len.length)
				if((this_read_length = recv(fds[this->fd_index].fd, &msg[read_length], len.length - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					printf("nope 6");
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			msg[read_length] = 0;

			while(read_length-- > -1)
				msg[read_length] ^= mask[read_length%4];

			// Sometimes the Tor Browser Bundle will randomly send "(null)"? I just skip these so that the browser doesn't spam the chat
			if(!strcmp("(null)", msg)){
				printf("nope null");
				goto threadpool_done_unlock;
			}

			else if(!strcmp("chat", msg)){
				char *message, *prepared_message;

				message = strchr(&msg[5], 0) + 1;
				message = strchr(message, 0) + 1;

				sprintf(buffer, "%i", this->fd_index);
				struct socketBST *sNode =  searchNode(sockets_root, buffer, strlen(buffer) + 1);
				struct socketIdentityBST *sIdentity = searchNode(sNode->identities, &msg[5], message - &msg[5]);

				if(sIdentity == NULL){
					sendToSocket("error\0Invalid identity", 22, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				prepared_message = malloc(strlen(message) + strlen(sIdentity->identity->name) + 28);
				sprintf(prepared_message, "chat%c%s%c%s%c%s", 0, sIdentity->identity->name, 0, sIdentity->identity->trip, 0, message);
				sendToRoom(prepared_message, strlen(message) + strlen(sIdentity->identity->name) + 27, sIdentity->identity->roomBST);
				free(prepared_message);

			}else if(!strcmp("join", msg)){
				unsigned char valid = 1;
				char *name = &msg[5], *room, *trip, *tmp;

				if(len.length < 7 || 6 + strlen(name) == len.length)
					valid = 0;

				if(valid && 7 + strlen(name) + strlen((room = &name[strlen(name) + 1])) == len.length)
					valid = 0;

				if(!valid){
					sendToSocket("error\0Invalid join!", 19, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				trip = &room[strlen(room) + 1];
				trip = trip;

				if(!strlen(name)){
					unsigned char tmplen = random() % (guest_suffix_max - guest_suffix_min);
					name = malloc(6 + guest_suffix_min + tmplen);
					strcpy(name, "Guest");
					randomSuffix(&name, guest_suffix_min + tmplen);
				}else{
					tmp = malloc(strlen(name) + 1);
					strcpy(tmp, name);
					name = tmp;
				}

	//			stringChop(&name, 22);

				if(!strlen(room)){
					unsigned char tmplen = random() % (room_suffix_max - room_suffix_min);
					room = malloc(5 + room_suffix_min + tmplen);
					strcpy(room, "Room");
					randomSuffix(&room, room_suffix_min + tmplen);
				}else{
					tmp = malloc(strlen(room) + 1);
					strcpy(tmp, room);
					room = tmp;
				}

	//			stringChop(&room, 40);
				struct roomBST *rNode = searchNode(rooms_root, room, strlen(room) + 1);

				if(rNode == NULL){
					rNode = malloc(sizeof(struct roomBST));
					rNode->name = room;
					rNode->rb = 0;
					rNode->first = NULL;
					rNode->identities = NULL;
					rNode->last = NULL;
					rNode->left = NULL;
					rNode->right = NULL;

					pthread_mutex_init(&rNode->identities_mutex, NULL);

					insertNode((void**)&rooms_root, rNode, &rooms_root_mutex);
				}else{
					// Make sure the name isn't already being used in that room...
					struct roomIdentityBST* tmp = rNode->identities;

					while(tmp != NULL){
						if(!strcmp(tmp->identity->name, name)){
							sendToSocket("error\0This username is already taken!", 37, fds[this->fd_index].fd);

							free(room);
							free(name);
							goto threadpool_done_unlock;
						}

						tmp = tmp->right;
					}
				}

				struct socketIdentityBST *sIdentity = malloc(sizeof(struct socketIdentityBST));
				struct roomIdentityBST *rIdentity = malloc(sizeof(struct roomIdentityBST));

				sIdentity->name_length = strlen(room) + strlen(name) + 2;
				sIdentity->name = malloc(sIdentity->name_length);
				sprintf(sIdentity->name, "%s%c%s", room, 0, name);

				sIdentity->rb = 0;
				sIdentity->left = NULL;
				sIdentity->right = NULL;

				sIdentity->identity = malloc(sizeof(struct identityNode));
				sIdentity->identity->name = name;
	//				sIdentity->identity->color = getNameColor(name);
				sIdentity->identity->color = "88F";
				strcpy(&(sIdentity->identity->trip[0]), "abcdefghijklmnopqrst");

				sprintf(buffer, "%i", this->fd_index);
				sIdentity->identity->socketBST = searchNode(sockets_root, buffer, strlen(buffer) + 1);
				sIdentity->identity->roomBST = rNode;

				memcpy(rIdentity, sIdentity, sizeof(struct socketIdentityBST));
				rIdentity->next = NULL;

				insertNode((void**)&rNode->identities, rIdentity, &rNode->identities_mutex);
				insertNode((void**)&sIdentity->identity->socketBST->identities, sIdentity, &sIdentity->identity->socketBST->identities_mutex);

				if(rNode->first == NULL)
					rNode->first = rIdentity;
				else
					rNode->last->next = rIdentity;

				rNode->last = rIdentity;

				sprintf(buffer, "joined%c%s%c%s%c%s", 0, room, 0, name, 0, sIdentity->identity->trip);
				sendToSocket(buffer, 29 + strlen(name) + strlen(room), fds[this->fd_index].fd);

				struct roomIdentityBST *users = rNode->first;
				while(users != NULL){
					sprintf(buffer, "hi%c%s%c%s%c%s%c%s", 0, users->identity->name, 0, users->identity->trip, 0, users->identity->color, 0, sIdentity->name);
					memcpy(&buffer[26 + strlen(users->identity->name) + strlen(users->identity->color)], sIdentity->name, sIdentity->name_length);
					sendToSocket(buffer, 25 + strlen(users->identity->name) + strlen(users->identity->color) + sIdentity->name_length, fds[this->fd_index].fd);

					users = users->next;
				}
			}else
				sendToSocket("error\0Unrecognized input!", 25, fds[this->fd_index].fd);

			free_and_continue:
			if(msg != NULL)
				free(msg);

			msg = NULL;

/*
				// Handle invite
				if(!strncmp("invite: ", msg, 8)){
					char *iId, *iName, *iRoom;

					iId = msg + 8;
					iName = strstr(iId, " ");
					if(iName == NULL)
						continue;

					iName[0] = 0;
					iName++;

					iRoom = strstr(iName, " ");
					if(iRoom == NULL)
						continue;

					iRoom[0] = 0;
					iRoom++;

					struct roomBST *rNode = searchRoom(iRoom);
					if(rNode == NULL)
						continue;

					struct identityBST *rIdentity = rNode->identities;
					while(rIdentity != NULL){
						if(!strcmp(rIdentity->identity->name, iName) && strlen(iName) == strlen(rIdentity->identity->name))
							break;

						rIdentity = rIdentity->right;
					}

					if(rIdentity == NULL)
						continue;

					struct identityBST *thisIdentity = searchIdentity(activeSocket->identities, iId);

					char *encoded;
					unsigned char offset;
					unsigned short messageLength = 30 + strlen(thisIdentity->identity->room->room);

					if(125 < messageLength){
						offset = 4;
						encoded = smalloc(messageLength + 4);

						encoded[0] = -127;
						encoded[1] = 126;
						encoded[2] = (unsigned char)(messageLength/256);
						encoded[3] = messageLength % 256;
					}else{
						offset = 2;
						encoded = smalloc(messageLength + 2);

						encoded[0] = -127;
						encoded[1] = messageLength;
					}

					// Invites don't really care about the exact identity invited, only the user...
					sprintf(&encoded[offset], "{\"invite\":\"%s\",\"event\":\"invite\"}", thisIdentity->identity->room->room);
					send(rIdentity->identity->socket->id, encoded, messageLength + offset, MSG_NOSIGNAL);

					free(encoded);

					continue;
				}

				// If a user wants to leave a room
				if(!strncmp("leave: ", msg, 7)){
					if(msg + 7 == NULL)
						continue;

					struct identityBST* sIdentity = searchIdentity(activeSocket->identities, msg + 7);

					if(sIdentity == NULL)
						continue;

					struct roomBST* rNode = sIdentity->identity->room;
					struct identityBST* rIdentity = searchIdentity(rNode->identities, msg + 7);

					removeIdentity(&rNode->identities, rIdentity);

					if(rNode->identities == NULL)
						removeRoom(rNode);
					else{
						char* bye;
						bye = smalloc(25 + strlen(sIdentity->identity->name));
						sprintf(bye, "{\"event\":\"bye\",\"user\":\"%s\"}", sIdentity->identity->name);
						sendToRoom(bye, sIdentity->identity->room->room);

						free(bye);
					}

					free(sIdentity->identity->color);
					free(sIdentity->identity->name);
					free(sIdentity->identity);

					removeIdentity(&activeSocket->identities, sIdentity);

					continue;
				}

				// Otherwise, if the payload starts with "chat: ", it's a normal message. Else, we disregard the packet
				if(strncmp("chat: ", msg, 6))
					continue;

				char *id = msg + 6;
				char *tmp = strstr(id, " ");

				if(tmp == NULL)
					continue;

				tmp[0] = 0;

				unsigned char offset = 7 + strlen(id);

				tmp = smalloc(len - offset);
				memcpy(tmp, &msg[offset], len - offset);
				tmp[len - offset] = 0;

				if(!strlen(tmp))
					continue;

				struct identityBST *identityNode = searchIdentity(activeSocket->identities, id);
				if(identityNode == NULL)
					continue;

	//			urlStringChop(&tmp, 350);

				char *b;
				b = smalloc(strlen(tmp) + strlen(identityNode->identity->name) + 36);
				sprintf(b, "{\"user\":\"%s\",\"event\":\"chat\",\"data\":\"%s\"}", identityNode->identity->name, tmp);
				sendToRoom(b, identityNode->identity->room->room);

				free(b);
				free(tmp);
			}
	*/
		}

		threadpool_done_unlock:
		this->fd_index = -1;
		pthread_mutex_unlock(&(this->mutex));
	}

	return NULL;
}
