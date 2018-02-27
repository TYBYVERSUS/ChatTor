
// This thread should handle PING/PONG and maybe other stuff
static void* hkFunc(){
//	struct timespec ping_pong_heartbeat_tv, ping_pong_response_tv;
//	ping_pong_heartbeat_tv.tv_sec = ping_pong_heartbeat_seconds;
//	ping_pong_heartbeat_tv.tv_nsec = ping_pong_heartbeat_milliseconds * 1000000;

//	ping_pong_response_tv.tv_sec = ping_pong_response_seconds;
//	ping_pong_response_tv.tv_nsec = ping_pong_response_milliseconds * 1000000;

	for(;;)
		pause();

	return NULL;
}

// A recursive function used in the signal handler to find the active socket
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

// Signal thread. Used to catch SIGIO, find the active socket, and then pass it on to the first avialballe thread in the threadpool
static void* sigFunc(void *arg){
	struct thread_pool *thread = (struct thread_pool*)arg;
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

// The threadpool function. This will run in parallel with itself. This is the "heavy lifting" for when a socket has activity
static void* poolFunc(void *arg){
	char buffer[4096] = {0};
	int caught_signal;

	struct thread_pool *this = arg;
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
			int new_socket = accept(listener_socket, NULL, NULL);
			if(new_socket == -1)
				goto threadpool_done_unlock;

			fcntl(new_socket, F_SETFL, fcntl(new_socket, F_GETFL) | O_ASYNC);
			fcntl(new_socket, F_SETOWN, getpid());

			recv(new_socket, buffer, 4095, 0);

			// Adding the socket to fds
			pthread_mutex_lock(&fds_mutex);

			this->fd_index = fd_counter;

			// Setting fd_counteir to the next open fd_index
			while(fds[++fd_counter].fd != 0){
				if(fd_counter >= fds_length){
					printf("TOO MANY SOCKETS! Exiting now...\n");
					exit(1);
				}
			}

			fds[this->fd_index].fd = new_socket;
			fds[this->fd_index].events = POLLIN;

			pthread_mutex_unlock(&fds_mutex);

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
			sprintf(buffer, "%i", fds[this->fd_index].fd);


			struct socketBST *socket_node = bstMakeSocketBST(buffer, strlen(buffer)+1, &bstNIL, new_socket);

			// &sockets_root_mutex was previously also sent to insert
			bstInsert((void*)socket_node, (void**)&sockets_root);

			goto threadpool_done_unlock;
		}

		// Else, if it's not the listener socket that has activity, we handle it here: https://tools.ietf.org/html/rfc6455#section-5
		while(poll(&fds[this->fd_index], 1, 0)){
			char mask[4];
			union websocket_frame_length len;

			ssize_t this_read_length, read_length = 0;
			while(read_length < 2)
				if((this_read_length = recv(fds[this->fd_index].fd, &buffer[read_length], 2 - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			printf("First 2: %hhu %hhu\n", (unsigned char)buffer[0], (unsigned char)buffer[1]);

			//   FIN & op = 1          Mask set
			if(buffer[0] != -127 || buffer[1] > -1){
				// If this is a cllose, it might be handled here? Test this please
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
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			// Preliminary check for message length before we allocate memory. We assume, worst case, everything is really long unicode
			if(len.length > max_msg_length * 4){
				while(recv(fds[this->fd_index].fd, buffer, 4096, 0) == 4096){}
				printf("Message too long with length: %"PRIu64"\n", len.length);
				goto threadpool_done_unlock;
			}

			char *msg = malloc(len.length + 1);

			read_length = 0;
			while((uint64_t)read_length < len.length)
				if((this_read_length = recv(fds[this->fd_index].fd, &msg[read_length], len.length - read_length, 0)) > 0)
					read_length += this_read_length;
				else{
					close_socket(this->fd_index);
					goto threadpool_done_unlock;
				}

			msg[read_length] = 0;

			while(read_length-- > -1)
				msg[read_length] ^= mask[read_length%4];

			// Sometimes the Tor Browser Bundle will randomly send "(null)"? I just skip these so that the browser doesn't spam the chat
			if(!strcmp("(null)", msg)){
				goto threadpool_done_unlock;
			}

			// If/Else ladder of commands
			else if(!strcmp("chat", msg)){
				// If it's a normal chat message...
				char *message, *prepared_message;

				message = strchr(&msg[5], 0) + 1;
				message = strchr(message, 0) + 1;

				sprintf(buffer, "%i", fds[this->fd_index].fd);
				struct socketBST *sNode =  bstSearch(buffer, strlen(buffer) + 1, (void*) sockets_root);
				struct socketIdentityBST *sIdentity = bstSearch(&msg[5], message - &msg[5], (void*) sNode->identities);

				if(sIdentity == &bstNIL){
					sendToSocket("error\0Invalid identity", 22, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				prepared_message = malloc(strlen(message) + strlen(sIdentity->identity->name) + 28);
				sprintf(prepared_message, "chat%c%s%c%s%c%s", 0, sIdentity->identity->name, 0, sIdentity->identity->trip, 0, message);
				sendToRoom(prepared_message, strlen(message) + strlen(sIdentity->identity->name) + 27, sIdentity->identity->roomBST);
				free(prepared_message);


			}else if(!strcmp("join", msg)){
				// If it's a user joining a new room
				char *room = &msg[5], *name, *tmp;

				if(len.length < 7 || 6 + strlen(room) == len.length){
					sendToSocket("error\0Invalid join!", 19, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				name = &room[strlen(room) + 1];

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
				struct roomBST *rNode = bstSearch(room, strlen(room) + 1, (void*) rooms_root);

				if(rNode == &bstNIL){
					rNode = bstMakeRoomBST(room, strlen(room)+1, &bstNIL);

					// previously rooms_root_mutex was also sent to the function
					bstInsert((void*) rNode, (void**)&rooms_root);
				}else{
					// Make sure the name isn't already being used in that room...
					struct roomIdentityBST* tmp = bstSearch(name, strlen(name)+1, rNode->identities);

					if(tmp != &bstNIL){
						sendToSocket("error\0This username is already taken!", 37, fds[this->fd_index].fd);

						free(room);
						free(name);
						goto threadpool_done_unlock;
					}
				}

				int tempKeySize = strlen(room) + strlen(name) + 2;
				char* tempKey = malloc(tempKeySize);
				sprintf(tempKey, "%s%c%s", room, 0, name);

				struct identityNode* tempIdentity = malloc(sizeof(struct identityNode));
				tempIdentity->name = malloc(strlen(name)+1);
				strcpy(tempIdentity->name, name);
	//				sIdentity->identity->color = getNameColor(name);
				tempIdentity->color = malloc(4);
				strcpy(tempIdentity->color, "88F");
				strcpy(&(tempIdentity->trip[0]), "abcdefghijklmnopqrst");

				
				sprintf(buffer, "%i", fds[this->fd_index].fd);

				tempIdentity->socketBST = bstSearch(buffer, strlen(buffer) + 1, (void*) sockets_root);
				tempIdentity->roomBST = rNode;

				// NOTE: currently sending the same tempIdentity to both sIdentity and rIdentity
				// When one or the other gets removed, tempIdentity will be freed and the pointer in the other will be broken
				// Consider making one malloced copy for each.
				struct socketIdentityBST *sIdentity = bstMakeSocketIdentityBST(tempKey, tempKeySize, tempIdentity);
				struct roomIdentityBST *rIdentity   = bstMakeRoomIdentityBST(  tempKey, tempKeySize, tempIdentity);

				// mutex previously also sent to function  &rNode->identities_mutex
				bstInsert(rIdentity, (void**)&rNode->identities);
				// mutex previously also sent to function  &sIdentity->identity->socketBST->identities_mutex
				bstInsert(sIdentity, (void**)&sIdentity->identity->socketBST->identities);

				free(tempKey);


				sprintf(buffer, "joined%c%s%c%s%c%s", 0, room, 0, name, 0, sIdentity->identity->trip);
				sendToSocket(buffer, 29 + strlen(name) + strlen(room), fds[this->fd_index].fd);

				struct roomIdentityBST *users = bstFirst(rNode);
				while(users != &bstNIL){
					sprintf(buffer, "hi%c%s%c%s%c%s%c%s", 0, users->identity->name, 0, users->identity->trip, 0, users->identity->color, 0, sIdentity->key);
					memcpy(&buffer[26 + strlen(users->identity->name) + strlen(users->identity->color)], sIdentity->key, sIdentity->keySize);
					sendToSocket(buffer, 25 + strlen(users->identity->name) + strlen(users->identity->color) + sIdentity->keySize, fds[this->fd_index].fd);

					users = bstNext(users);
				}

			}else if(!strcmp("invite", msg)){
				unsigned char valid = 1;
				char *room = &msg[7], *name, *to_room;

				if(len.length < 9 || 8 + strlen(room) == len.length)
					valid = 0;

				if(valid && 9 + strlen(room) + strlen((name = &room[strlen(room) + 1])) == len.length)
					valid = 0;

				if(!valid){
					sendToSocket("error\0Invalid invite!", 21, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				to_room = &name[strlen(name) + 1];

				if(!strcmp(room, to_room)){
					sendToSocket("error\0Can't invite to current room!", 35, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				// We need to verify that the room the other user is being invited to is a room that this current user is in
				sprintf(buffer, "%i", fds[this->fd_index].fd);
				struct socketBST* this_socket_node = bstSearch(buffer, strlen(buffer)+1, sockets_root);
				struct socketIdentityBST* socket_identity_tmp = bstSearch(to_room, strlen(to_room)+1, (void*) this_socket_node->identities);

				if(socket_identity_tmp == &bstNIL){
					sendToSocket("error\0Invalid room!", 19, fds[this->fd_index].fd);
					goto free_and_continue;
				}


				// Now we have to find the room/user they are trying to invite...
				struct roomBST* room_node = bstSearch(room, strlen(room) + 1, (void*) rooms_root);
				if(room_node == &bstNIL){
					sendToSocket("error\0Room not found!", 21, fds[this->fd_index].fd);
					goto free_and_continue;
				}
					
				//                                                           SHOULDN'T THIS BE +2 ?
				struct roomIdentityBST* identity_node = bstSearch(room, strlen(name) + strlen(room) + 1, (void*) room_node->identities);
				if(identity_node == &bstNIL){
					sendToSocket("error\0User not found!", 21, fds[this->fd_index].fd);
					goto free_and_continue;
				}

				// Then get the socket number and send the invite!
				sprintf(buffer, "invite%c%s", 0, to_room);
				sendToSocket(buffer, 7 + strlen(to_room), atoi(identity_node->identity->socketBST->key));

			}else
				// Else, probably someone trying to hack me
				sendToSocket("error\0Unrecognized input!", 25, fds[this->fd_index].fd);

			free_and_continue:
			if(msg != NULL){
				free(msg);
				msg = NULL;
			}

			// Code from legacy branch for handling invites/leaving. Here for easy reference
/*
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

		// Set this thread's fd_index to -1 and then unlock the thread mutex
		threadpool_done_unlock:
		this->fd_index = -1;
		pthread_mutex_unlock(&(this->mutex));
	}

	return NULL;
}
