struct identityNode;

struct identityBST{
	char index[17];
	unsigned char rb;
	struct identityNode *identity;
	struct identityBST *left, *right;
};

struct socketBST{
	int id;
	unsigned char rb;
	struct identityBST *identities;
	struct socketBST *left, *right;
};

struct roomBST{
	char* room;
	unsigned char rb;
	struct identityBST *identities;
	struct roomBST *left, *right;
};

struct identityNode{
	char *name, *color;
	struct socketBST *socket;
	struct roomBST *room;
};



struct socketBST *sRoot = NULL;
struct roomBST *rRoot = NULL;



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
}
