// Limit string to length characters. Return number of bytes
void stringChop(char **str, uint64_t length){
	uint64_t i, offset;

	if(strlen(*str) <= length)
		return;

	// The first byte of a unicode character will be >= 0xC2, and then the following bytes for that unicode character will be 0xBF >= byte >= 0x80
	for(i=0, offset=0; i < length && (*str)[offset] != 0;i++)
		if((unsigned char)(*str)[offset] >= 0xC2)
			while(0xBF >= (unsigned char)(*str)[++offset] && (unsigned char)(*str)[offset] >= 0x80){}
		else
			offset++;

	(*str)[offset] = 0;
}

// For sending a message to a single socket
void sendToSocket(char *msg, uint64_t length, int fd){
	unsigned char offset;
	char* encoded;

	union websocketFrameLength len;
	len.length = length;

	if(length < 125){
		offset = 2;
		encoded = malloc(length + 3);

		encoded[0] = -127;
		encoded[1] = length;
	}else if(length < 65536){
		offset = 4;
		encoded = malloc(length + 5);

		encoded[0] = -127;
		encoded[1] = 126;
		encoded[2] = len.bytes[1];
		encoded[3] = len.bytes[0];
	}else{
		offset = 10;
		encoded = malloc(length + 11);

		encoded[0] = -127;
		encoded[1] = 127;
		encoded[2] = len.bytes[7];
		encoded[3] = len.bytes[6];
		encoded[4] = len.bytes[5];
		encoded[5] = len.bytes[4];
		encoded[6] = len.bytes[3];
		encoded[7] = len.bytes[2];
		encoded[8] = len.bytes[1];
		encoded[9] = len.bytes[0];
	}

	memcpy(&encoded[offset], msg, length);

	send(fd, encoded, length + offset, 0);

	free(encoded);
}

// Send a message to a whole room
void sendToRoom(char *msg, uint64_t length, struct roomBST* rNode){
	struct roomIdentityBST *each = rNode->identities;
	while(each->left != NULL)
		each = each->left;

	while(each != NULL){
		union websocketFrameLength len;
		unsigned char offset;
		char *encoded;

		length += each->index_length;
		len.length = length;

		if(length < 125){
			offset = 2;
			encoded = malloc(length + 3);

			encoded[0] = -127;
			encoded[1] = length;
		}else if(length < 65536){
			offset = 4;
			encoded = malloc(length + 5);

			encoded[0] = -127;
			encoded[1] = 126;
			encoded[2] = len.bytes[1];
			encoded[3] = len.bytes[0];
		}else{
			offset = 10;
			encoded = malloc(length + 11);

			encoded[0] = -127;
			encoded[1] = 127;
			encoded[2] = len.bytes[7];
			encoded[3] = len.bytes[6];
			encoded[4] = len.bytes[5];
			encoded[5] = len.bytes[4];
			encoded[6] = len.bytes[3];
			encoded[7] = len.bytes[2];
			encoded[8] = len.bytes[1];
			encoded[9] = len.bytes[0];
		}

		length -= each->index_length;
		memcpy(&encoded[offset], msg, length);
		encoded[offset+length] = 0;

		memcpy(&encoded[length+offset+1], each->index, each->index_length);
		encoded[offset+length+each->index_length] = 0;
		send(each->identity->socket_node->fd, encoded, offset + length + each->index_length, 0);

		free(encoded);
		each = bstNext(each);
	}
}

// Close the socket
void close_socket(int fd_index){
	pthread_mutex_lock(&pollFDsMutex);

	shutdown(pollFDs[fd_index].fd, SHUT_RDWR);
	close(pollFDs[fd_index].fd);

	pollFDs[fd_index].fd = 0;
	fdCounter = fd_index;

	pthread_mutex_unlock(&pollFDsMutex);

	char buffer[6 + 20 + nameLimit];
	sprintf(buffer, "%i", fd_index);
	pthread_mutex_lock(&socketsRootMutex);
	struct socketBST* this_socket_node = bstSearch(buffer, strlen(buffer)+1, socketsRoot);
	pthread_mutex_unlock(&socketsRootMutex);

	struct identityNode *identity_node;
	struct roomIdentityBST *room_identity;
	struct socketIdentityBST *socket_identity;

	unsigned short index_length;
	while(this_socket_node->identities != NULL){
		index_length = strlen(this_socket_node->identities->index);
		index_length += strlen(&this_socket_node->identities->index[index_length + 1]) + 2;

		socket_identity = this_socket_node->identities;
		identity_node = socket_identity->identity;

		pthread_mutex_lock(&identity_node->room_node->identities_mutex);
		room_identity = bstSearch(this_socket_node->identities->index, index_length, identity_node->room_node->identities);
		pthread_mutex_unlock(&identity_node->room_node->identities_mutex);

		bstRemoveNode(room_identity, (void**)&identity_node->room_node->identities);
		bstRemoveNode(socket_identity, (void**)&this_socket_node->identities);

	
		if(identity_node->room_node->identities == NULL){
			bstRemoveNode(identity_node->room_node, (void**)&roomsRoot);
			pthread_mutex_destroy(&identity_node->room_node->identities_mutex);
			free(identity_node->room_node->index);
			free(identity_node->room_node);
		}else{
			sprintf(buffer, "bye%c%s%c%s", 0, identity_node->name, 0, identity_node->trip);
			sendToRoom(buffer, 5 + strlen(identity_node->name) + strlen(identity_node->trip), identity_node->room_node);
		}

		free(room_identity->index);
		free(room_identity);
		free(socket_identity); // The *index of both of these identity nodes point to the same memory address

		free(identity_node->name);
		free(identity_node->trip);
		free(identity_node->color);
		free(identity_node);
	}

	bstRemoveNode(this_socket_node, (void**)&socketsRoot);
	pthread_mutex_destroy(&this_socket_node->identities_mutex);
	free(this_socket_node->index);
	free(this_socket_node);
}

// A function for adding a random suffix to a string (b64 charset without + and /).
void randomSuffix(char **rand, unsigned long len){
	unsigned long c, prelen = strlen(*rand);

	for(c = 0; c < len; c++)
		(*rand)[prelen+c] = b64Table[random() % 62];

	(*rand)[prelen+len] = 0;
}

