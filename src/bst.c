struct identityNode;

struct roomIdentityBST{
	char *name;
	unsigned char rb;
	struct roomIdentityBST *left, *right;

	unsigned int name_length;
	struct identityNode *identity;

	struct roomIdentityBST *next;
};

struct socketIdentityBST{
	char *name;
	unsigned char rb;
	struct socketIdentityBST *left, *right;

	unsigned int name_length;
	struct identityNode *identity;
};

struct socketBST{
	char *name;
	unsigned char rb;
	struct socketBST *left, *right;

	int fd;
	struct socketIdentityBST *identities;
	pthread_mutex_t identities_mutex;
};

struct roomBST{
	char *name;
	unsigned char rb;
	struct roomBST *left, *right;

	struct roomIdentityBST *first, *identities, *last;
	pthread_mutex_t identities_mutex;
};

struct identityNode{
	char *color, *name, trip[21];
	struct socketBST *socketBST;
	struct roomBST *roomBST;
};

// This is used for typecasting void* for BST operations... Things MUST BE in this order
struct redBlackBSTBase{
	char *name;
	unsigned char rb;
	struct redBlackBSTBase *left, *right;
};


struct socketBST *sockets_root = NULL;
struct roomBST *rooms_root = NULL;


void insertNode(void **root, void *node, pthread_mutex_t *mutex){
	if(mutex != NULL)
		pthread_mutex_lock(mutex);

	((struct redBlackBSTBase*)node)->right = *root;
	*root = node;

	if(mutex != NULL)
		pthread_mutex_unlock(mutex);
}

void* searchNode(void *root, char *name, uint64_t length){
	struct redBlackBSTBase *tmp = (struct redBlackBSTBase*)root;

	while(tmp != NULL){
		if(!memcmp(tmp->name, name, length))
			return tmp;

		tmp = tmp->right;
	}

	return NULL;
}

/*
void insertSocket(struct socketBST *socket){
	if(sRoot == NULL){
		sRoot = socket;
		return;
	}

	struct socketBST *tmp = sRoot;
	while(tmp->right != NULL)
		tmp = tmp->right;

	tmp->right = socket;
}

void insertIdentity(struct identityBST **root, struct identityBST *identity){
	if(*root == NULL){
		*root = identity;
		return;
	}

	struct identityBST *tmp = *root;
	while(tmp->right != NULL)
		tmp = tmp->right;

	tmp->right = identity;
}

void insertRoom(struct roomBST *room){
	if(rRoot == NULL){
		rRoot = room;
		return;
	}

	struct roomBST *tmp = rRoot;
	while(tmp->right != NULL)
		tmp = tmp->right;

	tmp->right = room;
}
*/
/*
struct socketBST* searchSocket(int id){
	struct socketBST *tmp = sRoot;
	while(tmp != NULL){
		if(tmp->id == id)
			return tmp;

		tmp = tmp->right;
	}

	return NULL;
}

struct identityBST* searchIdentity(struct identityBST *root, char* id){
	struct identityBST *tmp = root;
	while(tmp != NULL){
		if(!strcmp(tmp->index, id))
			return tmp;

		tmp = tmp->right;
	}

	return NULL;
}

struct roomBST* searchRoom(char* room){
	struct roomBST *tmp = rRoot;
	while(tmp != NULL){
		if(!strcmp(tmp->room, room) && strlen(room) == strlen(tmp->room))
			return tmp;

		tmp = tmp->right;
	}

	return NULL;
}



void removeSocket(struct socketBST *socket){
	if(sRoot == NULL)
		return;

	struct socketBST* tmp = sRoot;
	if(sRoot == socket){
		sRoot = tmp->right;
		free(tmp);
		return;
	}

	while(tmp->right != socket && tmp->right != NULL)
		tmp = tmp->right;

	if(tmp->right == NULL)
		return;

	tmp->right = socket->right;
	free(socket);
}

void removeIdentity(struct identityBST **root, struct identityBST *identity){
	if(*root == NULL)
		return;

	struct identityBST* tmp = *root;

	if(*root == identity){
		*root = tmp->right;
		free(tmp);
		return;
	}

	while(tmp->right != identity && tmp->right != NULL)
		tmp = tmp->right;

	if(tmp->right == NULL)
		return;

	tmp->right = identity->right;
	free(identity);
}

void removeRoom(struct roomBST *room){
	if(rRoot == NULL)
		return;

	struct roomBST* tmp = rRoot;
	if(rRoot == room){
		rRoot = tmp->right;
		free(tmp->room);
		free(tmp);
		return;
	}


	while(tmp->right != room && tmp->right != NULL)
		tmp = tmp->right;

	if(tmp->right == NULL)
		return;

	tmp->right = room->right;
	free(room->room);
	free(room);
}*/
