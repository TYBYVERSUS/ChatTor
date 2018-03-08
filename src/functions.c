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

	union websocket_frame_length len;
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
	struct roomIdentityBST *each = bstFirst(rNode->identities);

	while(each != NULL){
		union websocket_frame_length len;
		unsigned char offset;
		char *encoded;

		printf("length: %"PRIu64"\n", length);

		length += each->keySize;
		len.length = length;

		printf("new length: %"PRIu64"\n", length);

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

		length -= each->keySize;
		printf("final length: %"PRIu64"\n", length);
		memcpy(&encoded[offset], msg, length);
		encoded[offset+length] = 0;

		memcpy(&encoded[length+offset+1], each->key, each->keySize);
		encoded[offset+length+each->keySize] = 0;
		send(each->identity->socketNode->fd, encoded, offset + length + each->keySize, 0);

		free(encoded);
		each = bstNext(each);
	}
}

// Close the socket
void close_socket(int fd_index){
	pthread_mutex_lock(&fds_mutex);

	shutdown(fds[fd_index].fd, SHUT_RDWR);
	close(fds[fd_index].fd);

	fds[fd_index].fd = 0;
	fd_counter = fd_index;

	pthread_mutex_unlock(&fds_mutex);

	char buffer[64];
	sprintf(buffer, "%i", fd_index);
	struct socketBST* this_socket_node = bstSearch(buffer, strlen(buffer)+1, sockets_root);

	unsigned short index_length;
	while(this_socket_node->identities != NULL){
		index_length = strlen(this_socket_node->identities->key);
		index_length += strlen(&this_socket_node->identities->key[index_length + 1]) + 2;

		struct identityNode *identity_node = this_socket_node->identities->identity;
		struct roomIdentityBST *room_identity = bstSearch(this_socket_node->identities->key, index_length, identity_node->roomNode->identities);

		bstRemoveNode(room_identity, (void**)&identity_node->roomNode->identities);
		bstRemoveNode(this_socket_node->identities, (void**)&this_socket_node->identities);

		if(identity_node->roomNode->identities == NULL)
			bstRemoveNode(identity_node->roomNode, (void**)&rooms_root);

		free(identity_node);
	}

	bstRemoveNode(this_socket_node, (void**)&sockets_root);
}

// A function for adding a random suffix to a string (b64 charset without + and /).
void randomSuffix(char **rand, unsigned long len){
	unsigned long c, prelen = strlen(*rand);

	for(c = 0; c < len; c++)
		(*rand)[prelen+c] = b64Table[random() % 62];

	(*rand)[prelen+len] = 0;
}

