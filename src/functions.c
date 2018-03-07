// Limit string to length characters. Return number of bytes
void stringChop(char **str, uint64_t length){
	uint64_t i, offset, len = strlen(*str);

	if(len <= length)
		return;

	// The first byte of a unicode character will be >= 0xC2, and then the following bytes for that unicode character will be 0xBF >= byte >= 0x80
	for(i=0, offset=0; i < length && (*str)[offset] != 0;i++){
		printf("%"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" %hhu %c\n", length, len, offset, i, (unsigned char)(*str)[offset], (*str)[offset]);

		if((unsigned char)(*str)[offset] >= 0xC2)
			while(0xBF >= (unsigned char)(*str)[++offset] && (unsigned char)(*str)[offset] >= 0x80){}
		else
			offset++;
	}

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

//	Actually clean up data structures. This will probably be unstable as is

//	struct socketBST *sNode = searchSocket(fd);

//	if(sNode == NULL)
//		return;

/*	struct identityBST *sIdentity = sNode->identities;
	while(sIdentity != NULL){
		struct roomBST *rNode = sIdentity->identity->room;
		struct identityBST *rIdentity = searchIdentity(rNode->identities, sIdentity->index);

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

		free(sIdentity->identity->name);
		free(sIdentity->identity->color);
		free(sIdentity->identity);

		removeIdentity(&sNode->identities, sIdentity);

		sIdentity = sNode->identities;
	}

	removeSocket(sNode);*/
}

// A function for adding a random suffix to a string (b64 charset without + and /).
void randomSuffix(char **rand, unsigned long len){
	unsigned long c, prelen = strlen(*rand);

	for(c = 0; c < len; c++)
		(*rand)[prelen+c] = b64Table[random() % 62];

	(*rand)[prelen+len] = 0;
}

