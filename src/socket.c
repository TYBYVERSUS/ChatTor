#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "bst.c"

// Todo
// Actually make the BST structs and functions work like real red-black BSTs. Right now, they are Linked Lists
// Add status feature? (may be possible client-side?)
// Put specific name colors in external file "colors.txt" or something to prevent recompiling to change name colors
// May be a small memory leak somewhere. My RAM usage keeps rising, although so does the number of users...
// Possible trip code feature? Still undecided on exact implenentation
// Still need to add PING/PONG. Spec is easy, implementation isn't so easy...

const static char* b64Table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned int nameSeed;
// s(realloc|malloc) are for strings. They both add +1 to the size of memory allocated
void* srealloc(void* ptr, size_t size){
	void *mem = realloc(ptr, size+1);
	if(!ptr || !mem){
		printf("realloc() error!");
		exit(1);
	}
	return mem;
}

void* smalloc(size_t size){
	void *mem = malloc(size+1);
	if(!mem){
		printf("malloc() failed!");
		exit(1);
	}
	return mem;
}

// Limits a url-encoded string to len characters (Note that this function modifies the string)
// Possibly add fix to not deal with URL-encoded strings and handle straight unicode? Username and room name are no longer passed via URL...
void urlStringChop(char** string, unsigned long len){
	unsigned long i, offset, stringlen = strlen(*string);

	if(stringlen <= len)
		return;

	for(i=0, offset=0; offset < stringlen && i <= len;){
		// If this character is a %, we have to see if this chracter is URL-encoded by checking if the next 2 characters are hex digits
		if(
			(*string)[offset] == 37
			&& (((*string)[offset+1] > 47 && ((*string)[offset+1] < 58)) || ((*string)[offset+1] > 64 && ((*string)[offset+1] < 71)) || ((*string)[offset+1] > 96 && ((*string)[offset+1] < 104)))
			&& (((*string)[offset+2] > 47 && ((*string)[offset+2] < 58)) || ((*string)[offset+2] > 64 && ((*string)[offset+2] < 71)) || ((*string)[offset+2] > 96 && ((*string)[offset+2] < 104)))
		){
			// Now, if this character is URL-encoded and the hex digits are between greater than 0x7F and less than 0xC0, this character is actually part of a unicode character and doesn't count towards the string total
			if(!((*string)[offset+1] == 56 || (*string)[offset+1] == 57 || (*string)[offset+1] == 65 || (*string)[offset+1] == 66 || (*string)[offset+1] == 97 || (*string)[offset+1] == 98))
				i++;
			
			if(i <= len)
				offset += 2;
		}else
		// Else the character is just normal ASCII and counts towards the total
			i++;

		if(i <= len)
			offset++;
	}

	if(i <= len)
		return;

	// Finally, if the string is too long, need to create a new char* of the appropriate length
	// Possible memory leak here?
	char* tmp = smalloc(offset);
	strncpy(tmp, *string, offset);
	tmp[offset] = 0;

	*string = tmp;
}

// Sends the message to everyone in the room
void sendToRoom(char *msg, char *room){
	unsigned short len = (unsigned short)strlen(msg) + 16;
	unsigned char offset;

	// Sorry, I'm too lazy to add support messages over 65535 bytes
	// Should I create a separate encode function? This code is used in a couple of places...
	if(!len || len > 65535)
		return;

	char *encoded;

	if(125 < len){
		offset = 4;
		encoded = smalloc(len+20);

		encoded[0] = -127;
		encoded[1] = 126;
		encoded[2] = (unsigned char)(len/256);
		encoded[3] = len % 256;
	}else{
		offset = 2;
		encoded = smalloc(len+18);

		encoded[0] = -127;
		encoded[1] = len;
	}

	memcpy(&encoded[offset+16], msg, len-16);
	len += offset;

	encoded[len] = 0;

	struct roomBST *rNode = searchRoom(room);
	struct identityBST *each = rNode->identities;
	while(each != NULL){
		memcpy(&encoded[offset], each->index, 16);
		send(each->identity->socket->id, encoded, len, MSG_NOSIGNAL);
		each = each->right;
	}
}

// This is the function to call when a connection has been closed
void close_socket(int fd){
	close(fd);

	struct socketBST *sNode = searchSocket(fd);

	if(sNode == NULL)
		return;

	struct identityBST *sIdentity = sNode->identities;
	while(sIdentity != NULL){
		struct roomBST *rNode = sIdentity->identity->room;
		struct identityBST *rIdentity = searchIdentity(rNode->identities, sIdentity->index);

		// Possibly not freeing everything properly here? Possible memory leak?
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

		removeIdentity(&sNode->identities, sIdentity);
		free(sIdentity->identity->name);
		free(sIdentity->identity->color);
		free(sIdentity->identity);
		sIdentity = sNode->identities;
	}

	removeSocket(sNode);
}

char* getNameColor(char* name){
	char* color;

/* Example of a custom name color

	if(!strcmp(name, "example")){
		color = smalloc(7);
		strcpy(color, "#FFFFFF");
		return color;
	}

*/
	
	unsigned int seed = nameSeed;
	unsigned short i, len = strlen(name);

	if(len > 32)
		for(i=0; i<len; i++)
			seed += ((unsigned char)name[i] + i) * 65536;
	else
		for(i=32; i>0; i--)
			seed += (unsigned char)name[i%(len+1)] * ((i+1) * (i+2));

	srand(seed);

	color = smalloc(7);
	sprintf(color, "#%X", rand()%191 + 64 + (rand()%191 + 64) * 0x100 + (rand()%191 + 64) * 0x10000);

	// Don't forget to reseed rand()! This prevents an attacker from setting knowable room/guest names
	struct timespec timespecSeed;
	clock_gettime(CLOCK_REALTIME, &timespecSeed);

	srand(((unsigned int)clock() + (unsigned int)timespecSeed.tv_nsec) * 256 * rand());

	return color;
}

// A function for adding a random suffix to a string (b64 charset without + and /)
void randomSuffix(char** rand, unsigned long len){
	unsigned long c, prelen = strlen(*rand);

	*rand = srealloc(*rand, len+prelen);
	for(c = 0; c < len; c++)
		(*rand)[prelen+c] = b64Table[random() % 62];

	(*rand)[prelen+len] = 0;
}

int main(int argc, char *argv[]){
	if(argc != 2){
		printf("usage is ./socket <port #>\n");
		exit(1);
	}

	unsigned char e, lsocket, val = 1;
	struct sockaddr_in *in_addr, serv_addr;
	struct epoll_event event, *events;
	socklen_t in_len = sizeof in_addr;

	// Bind the port
	// Don't know much about sockets. Perhaps I can do something differently here to make handling the sockets easier?
	lsocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if(lsocket < 0){
		printf("Socket creation failed");
		return 1;
	}

	if(setsockopt(lsocket, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), &val, sizeof(int)) < 0){
		printf("Socket creation failed");
		return 1;
	}

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[1]));
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(lsocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		printf("Error binding the socket");
		return 1;
	}

	listen(lsocket, 8);

	// Set up epoll
	e = epoll_create1(0);
	events = malloc(sizeof event);
	if(e == -1){
		printf("Epoll failed to create?");
		return 1;
	}

	event.data.fd = lsocket;
	event.events = EPOLLIN | EPOLLET;
	if(epoll_ctl(e, EPOLL_CTL_ADD, lsocket, &event) == -1){
		printf("epoll_ctl() failed for broadcast socket");
		return 1;
	}

	// Perhaps make this global and set time on every socket action? Timestamp is used in a few places...
	struct timespec timespecSeed;
	clock_gettime(CLOCK_REALTIME, &timespecSeed);

	nameSeed = ((unsigned int)clock() + (unsigned int)timespecSeed.tv_nsec) * 256 * rand();
	srand(nameSeed);

	// Now drops to nobody privileges! Should make own user in perfect world, but I can deal with that later...
	if(setgid(65534) != 0){
		printf("Unable to drop group privileges!");
		exit(0);
	}

	if(setuid(65534) != 0){
		printf("Unable to drop user privileges!");
		exit(0);
	}

	// This should be the only output that prints on a successful process start
	printf("Starting main loop...\n");

	while(epoll_wait(e, events, 1, -1) > 0){
		// Sometimes something happens and we need to close the socket
		if((events[0].events & EPOLLERR) || (events[0].events & EPOLLHUP) || (!(events[0].events & EPOLLIN)))
			close_socket(events[0].data.fd);

		else if(lsocket == events[0].data.fd){
			// Else, if the active socket is the broadcast socket, then we have a new connection
			// I can handle this so much better, but I don't have the time to fix this right now. Sorry :(
			char buffer[512]={0}, tmp[256]={0}, *nl;
			short len = 1;

			// Accepting the new socket
			event.data.fd = accept(lsocket, (struct sockaddr *) &in_addr, &in_len);
			fcntl(event.data.fd, F_SETFL, fcntl(event.data.fd, F_GETFL, 0) | O_NONBLOCK);

			if(event.data.fd == -1)
				break;

			if(epoll_ctl(e, EPOLL_CTL_ADD, event.data.fd, &event) == -1){
				close(event.data.fd);
				return 1;
			}

			// Now we have to read the headers. Did this incorrectly in the past, and will handle this properly some day...
			len = recv(event.data.fd, buffer, 511, MSG_WAITALL);

			if(len == 511 || len < 1){
				while(recv(event.data.fd, buffer, 511, MSG_WAITALL) > 0){}

				close(event.data.fd);
				continue;
			}

			nl = buffer;
			// If you have questions about this part, refer to the spec: https://tools.ietf.org/html/rfc6455
			// And find Sec-WebSocket-Key. We have to do a case-insensitive search because sometimes the header isn't capitalized properly...
			while((nl = strstr(nl, "\r\n")) != NULL){
				if(!strncasecmp(nl, "\r\nSec-WebSocket-Key: ", 21) && strlen(nl) >= 45){
					nl[45] = 0;
					sprintf(tmp, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", &nl[21]);
					break;
				}
				nl += 2;
			}

			if(!&tmp[0]){
				close(event.data.fd);
				continue;
			}

			// Now we hash the tmp buffer with SHA1
			SHA1((const unsigned char*)tmp, 60, (unsigned char*)tmp);
			strcpy(buffer, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ");
			
			// And base64_encode it
			buffer[97] = b64Table[(unsigned char)tmp[0] >> 2];
			buffer[98] = b64Table[((0x3&(unsigned char)tmp[0])<<4) + ((unsigned char)tmp[1]>>4)];
			buffer[99] = b64Table[((0x0f&(unsigned char)tmp[1])<<2) + ((unsigned char)tmp[2]>>6)];
			buffer[100] = b64Table[0x3f&(unsigned char)tmp[2]];

			buffer[101] = b64Table[(unsigned char)tmp[3] >> 2];
			buffer[102] = b64Table[((0x3&(unsigned char)tmp[3])<<4) + ((unsigned char)tmp[4]>>4)];
			buffer[103] = b64Table[((0x0f&(unsigned char)tmp[4])<<2) + ((unsigned char)tmp[5]>>6)];
			buffer[104] = b64Table[0x3f&(unsigned char)tmp[5]];

			buffer[105] = b64Table[(unsigned char)tmp[6] >> 2];
			buffer[106] = b64Table[((0x3&(unsigned char)tmp[6])<<4) + ((unsigned char)tmp[7]>>4)];
			buffer[107] = b64Table[((0x0f&(unsigned char)tmp[7])<<2) + ((unsigned char)tmp[8]>>6)];
			buffer[108] = b64Table[0x3f&(unsigned char)tmp[8]];

			buffer[109] = b64Table[(unsigned char)tmp[9] >> 2];
			buffer[110] = b64Table[((0x3&(unsigned char)tmp[9])<<4) + ((unsigned char)tmp[10]>>4)];
			buffer[111] = b64Table[((0x0f&(unsigned char)tmp[10])<<2) + ((unsigned char)tmp[11]>>6)];
			buffer[112] = b64Table[0x3f&(unsigned char)tmp[11]];

			buffer[113] = b64Table[(unsigned char)tmp[12] >> 2];
			buffer[114] = b64Table[((0x3&(unsigned char)tmp[12])<<4) + ((unsigned char)tmp[13]>>4)];
			buffer[115] = b64Table[((0x0f&(unsigned char)tmp[13])<<2) + ((unsigned char)tmp[14]>>6)];
			buffer[116] = b64Table[0x3f&(unsigned char)tmp[14]];

			buffer[117] = b64Table[(unsigned char)tmp[15] >> 2];
			buffer[118] = b64Table[((0x3&(unsigned char)tmp[15])<<4) + ((unsigned char)tmp[16]>>4)];
			buffer[119] = b64Table[((0x0f&(unsigned char)tmp[16])<<2) + ((unsigned char)tmp[17]>>6)];
			buffer[120] = b64Table[0x3f&(unsigned char)tmp[17]];

			buffer[121] = b64Table[(unsigned char)tmp[18] >> 2];
			buffer[122] = b64Table[((0x3&(unsigned char)tmp[18])<<4) + ((unsigned char)tmp[19]>>4)];
			buffer[123] = b64Table[((0x0f&(unsigned char)tmp[19])<<2)];

			strcpy(&buffer[124], "=\r\n\r\n");

			if(strlen(buffer) == 0 || send(event.data.fd, buffer, 129, MSG_NOSIGNAL) < 1){
				close_socket(event.data.fd);
				continue;
			}

			// Insert socket "BST" node
			struct socketBST *socketNode = malloc(sizeof(struct socketBST));
			socketNode->id = event.data.fd;
			socketNode->rb = 0;
			socketNode->identities = NULL;
			socketNode->left = NULL;
			socketNode->right = NULL;

			insertSocket(socketNode);
		}else{
			// Else, it's just a regular message
			// Could probably implement this better. I just re-wrote most of this code and haven't been able to comb back through it and improve it yet
			char buffer[4096]={0}, mask[4]={0}, *msg = NULL;
			long recvlen = 1;
			short tmplen;
			unsigned short i, len;

			// For example, this probably shouldn't be an infinite loop
			for(;;){
				// And there's probably a better way to handle my memory than this...
				if(msg != NULL){
					free(msg);
					msg = NULL;
				}

				// Refer to the spec for this part! We read the first 2 bytes of the packet header...
				recvlen = recv(events[0].data.fd, buffer, 2, 0);

				if(recvlen < 1)
					break;

				msg = malloc(2);
				memcpy(msg, buffer, recvlen);

				while(recvlen < 2){
					tmplen = recv(events[0].data.fd, buffer, 1, 0);

					if(tmplen < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
						break;

					if(tmplen == 1){
						msg[1] = buffer[0];
						recvlen++;
					}
				}

				if(recvlen != 2)
					break;

				len = ((unsigned char)msg[1]) % 128;

				//  FIN & op = 1       Mask set         len > 0
				if(msg[0] != -127 || msg[1] > -1 || msg[1] == -128){
					while(recv(events[0].data.fd, buffer, 4096, 0) > 0){}
					close_socket(events[0].data.fd);
					break;
				}

				len = ((unsigned char)msg[1]) % 128;

				free(msg);
				msg = NULL;

				// Again, no lengths over 65535. I'm lazy :(
				if(len == 127)
					break;

				if(len == 126){
					// If the length is set to 126, the next 2 bytes of the packet header contain the 16-bit payload length
					recvlen = recv(events[0].data.fd, buffer, 6, 0);
					msg = malloc(6);

					if(recvlen > 0)
						memcpy(msg, buffer, recvlen);
					else
						recvlen = 0;

					while(recvlen < 6){
						unsigned char readlen = 6 - recvlen;
						tmplen = recv(events[0].data.fd, buffer, readlen, 0);

						if(tmplen < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
							break;

						if(tmplen > 0){
							memcpy(msg + recvlen, buffer, tmplen);
							recvlen += tmplen;
						}
					}

					if(recvlen != 6)
						break;

					len = (((unsigned char)msg[0]) << 8) + (unsigned char)msg[1];

					memcpy(mask, &msg[2], 4);
				}else{
					// Else, if the length is set to 125 or less, this is the actual payload length
					recvlen = recv(events[0].data.fd, buffer, 4, 0);
					msg = malloc(4);

					if(recvlen > 0)
						memcpy(msg, buffer, recvlen);
					else
						recvlen = 0;

					while(recvlen < 4){
						unsigned char readlen = 4 - recvlen;
						tmplen = recv(events[0].data.fd, buffer, readlen, 0);

						if(tmplen < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
							break;

						if(tmplen > 0){
							memcpy(msg + recvlen, buffer, tmplen);
							recvlen += tmplen;
						}
					}

					if(recvlen != 4)
						break;

					memcpy(mask, msg, 4);
				}

				free(msg);

				// Now to read the actual payload (chat message)
				recvlen = 0;
				msg = smalloc(len);

				while(recvlen < len){
					unsigned short readlen = 4095;
					if(len - recvlen < readlen)
						readlen = len - recvlen;

					tmplen = recv(events[0].data.fd, buffer, readlen, 0);

					if(tmplen < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
						break;

					if(tmplen > 0){
						memcpy(msg + recvlen, buffer, tmplen);
						recvlen += tmplen;
					}
				}

				for(i=0; i < len; i++)
					msg[i] ^= mask[i%4];

				msg[len] = 0;

				// Sometimes the Tor Browser Bundle will randomly send "(null)"? I just skip these so that the browser doesn't spam the chat
				if(!strcmp("(null)", msg) && len == 6)
					continue;

				// A user wants to join a room. Values are delimited with a space and blank values will be replaced with random values
				if(!strncmp("join: ", msg, 6) && len >= 7){
					char *name, *room;
					room = msg + 6;
					name = strstr(room, " ");
					if(name == NULL)
						continue;

					name[0] = 0;
					name++;

					if(!strlen(room)){
						room = smalloc(4);
						strcpy(room, "Room");
						randomSuffix(&room, 36);
					}
					urlStringChop(&room, 40);

					if(!strlen(name)){
						name = smalloc(5);
						strcpy(name, "Guest");
						randomSuffix(&name, 17);
					}
					urlStringChop(&name, 22);

					struct socketBST *sNode = searchSocket(events[0].data.fd);
					if(sNode == NULL)
						continue;

					struct roomBST *rNode = searchRoom(room);
					if(rNode == NULL){
						rNode = malloc(sizeof(struct roomBST));
						rNode->room = smalloc(strlen(room));
						strcpy(rNode->room, room);
						rNode->rb = 0;

						rNode->identities = NULL;
						rNode->left = NULL;
						rNode->right = NULL;

						insertRoom(rNode);
					}else{
						// Make sure the name isn't already being used in that room...
						struct identityBST* tmp = rNode->identities;
						unsigned char taken = 0;

						while(tmp != NULL){
							if(strcmp(tmp->identity->name, name) == 0 && strlen(name) == strlen(tmp->identity->name)){
								char *encoded = smalloc(59);
								encoded[0] = -127;
								encoded[1] = 57;
								memcpy(&encoded[2], "{\"event\":\"error\",\"msg\":\"This username is already taken!\"}", 57);

								send(events[0].data.fd, encoded, 59, MSG_NOSIGNAL);

								free(encoded);
								taken = 1;
								break;
							}

							tmp = tmp->right;
						}

						if(taken)
							continue;
					}

					char *b = smalloc(4);
					strcpy(b, "id-");
					randomSuffix(&b, 13);

					struct identityBST *rIdentity = malloc(sizeof(struct identityBST)), *sIdentity = malloc(sizeof(struct identityBST));
					strcpy(sIdentity->index, b);
					sIdentity->rb = 0;
					sIdentity->identity = NULL;
					sIdentity->left = NULL;
					sIdentity->right = NULL;

					sIdentity->identity = malloc(sizeof(struct identityNode));
					sIdentity->identity->name = smalloc(strlen(name));
					strcpy(sIdentity->identity->name, name);
					sIdentity->identity->color = getNameColor(name);
					sIdentity->identity->socket = sNode;
					sIdentity->identity->room = rNode;

					*rIdentity = *sIdentity;

					free(b);
					b = smalloc(strlen(name) + strlen(sIdentity->identity->color) + 35);
					sprintf(b, "{\"user\":\"%s\",\"color\":\"%s\",\"event\":\"hi\"}", name, sIdentity->identity->color);
					sendToRoom(b, room);
					free(b);

					insertIdentity(&(rNode->identities), rIdentity);
					insertIdentity(&(sNode->identities), sIdentity);

					b = smalloc(63 + strlen(room) + strlen(name));
					sprintf(b, "%s{\"event\":\"joined\",\"room\":\"%s\",\"user\":\"%s\",\"users\":{", sIdentity->index, room, name);

					struct identityBST *users = rNode->identities;
					while(users != NULL){
						b = srealloc(b, strlen(b) + strlen(users->identity->name) + strlen(users->identity->color) + 7);
						sprintf(b + strlen(b), "\"%s\":\"%s\",", users->identity->name, users->identity->color);

						users = users->right;
					}

					strcpy(&b[strlen(b)-1], "}}");

					// Again, encoding a packet, but sending it only to this single socket
					char *encoded;
					unsigned short len = (unsigned short)strlen(b);

					if(125 < len){
						if(65536 < len)
							continue;

						encoded = smalloc(len+4);
						encoded[0] = -127;
						encoded[1] = 126;
						encoded[2] = (unsigned char)(len/256);
						encoded[3] = len % 256;
						memcpy(&encoded[4], b, len);
						len += 4;
					}else{
						encoded = smalloc(len+2);
						encoded[0] = -127;
						encoded[1] = len;
						memcpy(&encoded[2], b, len);
						len += 2;
					}

					encoded[len] = 0;

					send(events[0].data.fd, encoded, len, MSG_NOSIGNAL);

					free(b);
					free(encoded);
					continue;
				}

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

					struct socketBST *thisSocket = searchSocket(events[0].data.fd);
					struct identityBST *thisIdentity = searchIdentity(thisSocket->identities, iId);

					char *encoded;
					encoded = smalloc(strlen(thisIdentity->identity->room->room) + 32);
					encoded[0] = -127;
					encoded[1] = strlen(thisIdentity->identity->room->room)+30;
					// Invites don't really care about the exact identity invited, only the user...
					sprintf(&encoded[2], "{\"invite\":\"%s\",\"event\":\"invite\"}", thisIdentity->identity->room->room);
					send(rIdentity->identity->socket->id, encoded, strlen(thisIdentity->identity->room->room)+32, MSG_NOSIGNAL);
					free(encoded);
					continue;
				}

				// If a user wants to leave a room
				if(!strncmp("leave: ", msg, 7)){
					if(msg + 7 == NULL)
						continue;

					struct socketBST* sNode = searchSocket(events[0].data.fd);
					struct identityBST* sIdentity = searchIdentity(sNode->identities, msg + 7);

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

					removeIdentity(&sNode->identities, sIdentity);
					free(sIdentity->identity->name);
					free(sIdentity->identity);
					continue;
				}

				// Otherwise, if the payload starts with "chat: ", it's a normal message. Else, we disregard the packet
				if(strncmp("chat: ", msg, 6))
					continue;

				struct socketBST *socketNode = searchSocket(events[0].data.fd);
				if(socketNode == NULL)
					continue;

				char *id = msg + 6;
				char *tmp = strstr(id, " ");
				tmp[0] = 0;

				unsigned char offset = 7 + strlen(id);

				tmp = smalloc(len - offset);
				memcpy(tmp, &msg[offset], len - offset);
				tmp[len - offset] = 0;

				if(!strlen(tmp))
					continue;

				struct identityBST *identityNode = searchIdentity(socketNode->identities, id);
				if(identityNode == NULL)
					continue;

				urlStringChop(&tmp, 350);

				char *b;
				b = smalloc(strlen(tmp) + strlen(identityNode->identity->name) + 36);
				sprintf(b, "{\"user\":\"%s\",\"event\":\"chat\",\"data\":\"%s\"}", identityNode->identity->name, tmp);
				sendToRoom(b, identityNode->identity->room->room);
				free(b);
				free(tmp);
			}

			// Messy messy messy...
			if(msg != NULL)
				free(msg);

		}
	}

	return 0;
}
