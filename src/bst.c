// Various Binary Search Trees and functions to modify/navigate them

// This is definitely broken right now. I will fix this next

enum rbColors {bstBlack, bstRed};

// Should only be used for typecasting?
struct bstNode {
	char *key;
	unsigned int keySize; // Includes the terminating NULL byte
	unsigned char color;
	struct bstNode *left, *right, *parent;
};

struct roomIdentityBST;
struct socketIdentityBST;
struct socketBST;
struct roomBST;
struct identityNode;

// Global trees
struct socketBST *sockets_root = NULL;
struct roomBST *rooms_root = NULL;



// Useful functions:
// Search for key in bst. Returns NULL if not found. O(log n)
void* bstSearch(char* key, unsigned int keySize, void* root);

// Properly mallocs a new bst-node. Use this before bstInsert
struct roomIdentityBST* bstMakeRoomIdentityBST(char* key, unsigned int keySize, struct identityNode *identity);
struct socketIdentityBST* bstMakeSocketIdentityBST(char* key, unsigned int keySize, struct identityNode *identity);
struct roomBST* bstMakeRoomBST(char* key, unsigned int keySize, struct roomIdentityBST* identities);
struct socketBST* bstMakeSocketBST(char* key, unsigned int keySize, struct socketIdentityBST* identities, int fd);

// RB-BST insert. O(log n) Inserted nodes will be painted red. Does not allow duplicates
// Use a proper make function first on newNode
void bstInsert(void* newNode, void** treeRoot);

// Removes and properly frees removed node based on the key and the keysize
void bstRemoveRoomIdentityBST(char *key, unsigned int keySize, struct roomIdentityBST** root);
void bstRemoveSocketIdentityBST(char *key, unsigned int keySize, struct socketIdentityBST** root);
void bstRemoveSocketBST(char *key, unsigned int keySize, struct socketBST** root);
void bstRemoveRoomBST(char *key, unsigned int keySize, struct roomBST** root);

// RB-Bst remove, O(log n). NOTE: does not free the deleted node. Call bstFreeNode for that
void bstRemoveNode(void* x, void** root);

// Calls f on each node in sorted ascending order by their keys. O(n)
void bstInorderTraversal(void* root, void (*f)(void*));
void bstForEach(void* root, void (*f)(void*));

// Calls f on each node in post-order. O(n)
void bstPostOrderTraversal(void* root, void (*f)(void*));

// Gets the inorder successor. Next node in sorted order. O(log n). For k succesive calls: O(log(n) + k)
void* bstSuccessor(void* x);
void* bstNext(void* x);

// Gets the inorder predecessor. Previous node in sorted order. O(log n). For k succesive calls: O(log(n) + k)
void* bstPredecessor(void* x);
void* bstPrevious(void* x);

// Find bstNode with minimum/maximum key. That is, the first/last node in the tree. O(log n)
void* bstMinimum(void* x);
void* bstFirst(void* x);

void* bstMaximum(void* x);
void* bstLast(void* x);


struct socketBST{
	char *key;
	unsigned int keySize;
	enum rbColors color;
	struct socketBST *left, *right, *parent;

	int fd;
	struct socketIdentityBST *identities;
	pthread_mutex_t identities_mutex;
};
struct roomBST{
	char *key;
	unsigned int keySize;
	enum rbColors color;
	struct roomBST *left, *right, *parent;

	struct roomIdentityBST *first, *last, *identities;
	pthread_mutex_t identities_mutex;
};

struct socketIdentityBST{
	char *key;
	unsigned int keySize;
	enum rbColors color;
	struct socketIdentityBST *left, *right, *parent;

	struct identityNode *identity;
};
struct roomIdentityBST{
	char *key;
	unsigned int keySize;
	enum rbColors color;
	struct roomIdentityBST *left, *right, *parent;

	struct roomIdentityBST *next;
	struct identityNode *identity;
};

struct identityNode{
	char *color, *name, trip[21];
	struct socketBST *socketNode;
	struct roomBST *roomNode;
};



// Make sure stuff gets freed properly when removed
void bstFreeBstNode(void* x){
	if(x == NULL)
		return;

	struct bstNode* n = (struct bstNode*) x;
	free(n->key);
	free(n);
}
void bstFreeRoomSocketIdentityNode(void* x){
	if(x == NULL)
		return;

	struct roomIdentityBST* n = (struct roomIdentityBST*) x;
	free(n->identity->color);
	free(n->identity->name);
	free(n->identity);
}
void bstRemoveRoomIdentityBST(char *key, unsigned int keySize, struct roomIdentityBST** root){
	void* x = bstSearch(key, keySize, (void*) *root);
	if(x == NULL)
		return;

	bstRemoveNode(x, (void**) root);
	bstFreeRoomSocketIdentityNode(x);
	bstFreeBstNode(x);
}
void bstRemoveSocketIdentityBST(char *key, unsigned int keySize, struct socketIdentityBST** root){
	void* x = bstSearch(key, keySize, (void*) *root);
	if(x == NULL)
		return;

	bstRemoveNode(x, (void**) root);
	bstFreeRoomSocketIdentityNode(x);
	bstFreeBstNode(x);
}

void bstRemoveSocketBST(char *key, unsigned int keySize, struct socketBST** root){
	struct socketBST* x = (struct socketBST*) bstSearch(key, keySize, (void*) *root);
	if(x == NULL)
		return;

	bstRemoveNode((void*) x, (void**) root);
	// Must be postorder, not inorder or foreach
	bstPostOrderTraversal(x->identities, bstFreeRoomSocketIdentityNode);
	bstPostOrderTraversal(x->identities, bstFreeBstNode);
	bstFreeBstNode(x);
}

void bstRemoveRoomBST(char *key, unsigned int keySize, struct roomBST** root){
	struct roomBST* x = (struct roomBST*) bstSearch(key, keySize, (void*) *root);
	if(x == NULL)
		return;

	bstRemoveNode((void*) x, (void**) root);
	// Must be postorder, not inorder or foreach
	bstPostOrderTraversal(x->identities, bstFreeRoomSocketIdentityNode);
	bstPostOrderTraversal(x->identities, bstFreeBstNode);
	bstFreeBstNode(x);
}

// Make sure things are malloced solely for the tree so that they aren't freed randomly
struct socketBST* bstMakeSocketBST(char* key, unsigned int keySize, struct socketIdentityBST* identities, int fd){
	struct socketBST* newNode = malloc(sizeof(struct socketBST));
	newNode->keySize = keySize;
	newNode->identities = identities;
	pthread_mutex_init(&newNode->identities_mutex, NULL);
	newNode->fd = fd;
	newNode->key = malloc(keySize);
	memcpy(newNode->key, key, keySize);
	return newNode;
}
struct roomBST* bstMakeRoomBST(char* key, unsigned int keySize, struct roomIdentityBST* identities){
	struct roomBST* newNode = malloc(sizeof(struct roomBST));
	newNode->keySize = keySize;
	newNode->identities = identities;
	pthread_mutex_init(&newNode->identities_mutex, NULL);
	newNode->key = malloc(keySize);
	memcpy(newNode->key, key, keySize);
	newNode->first = NULL;
	newNode->last = NULL;
	return newNode;
}

struct socketIdentityBST* bstMakeSocketIdentityBST(char* key, unsigned int keySize, struct identityNode *identity){
	struct socketIdentityBST* newNode = malloc(sizeof(struct socketIdentityBST));
	newNode->keySize = keySize;
	newNode->identity = identity;
	newNode->key = malloc(keySize);
	memcpy(newNode->key, key, keySize);
	return newNode;
}
struct roomIdentityBST* bstMakeRoomIdentityBST(char* key, unsigned int keySize, struct identityNode *identity){
	struct roomIdentityBST* newNode = malloc(sizeof(struct roomIdentityBST));
	newNode->keySize = keySize;
	newNode->identity = identity;
	newNode->key = malloc(keySize);
	memcpy(newNode->key, key, keySize);
	return newNode;
}



// --------- Below is the red black implementation. Venture down on your own peril. --------- //

// Calls f on each node in sorted ascending order by their keys. O(n)
void bstInorderTraversal(void* treeRoot, void (*f)(void*)){
	if(treeRoot == NULL)
		return;

	struct bstNode* root = (struct bstNode*) treeRoot;
	bstInorderTraversal(root->left, f);
	f(treeRoot);
	bstInorderTraversal(root->right, f);	
}

void bstForEach(void* root, void (*f)(void*)){
	bstInorderTraversal(root, f);
}

void bstPostOrderTraversal(void* treeRoot, void (*f)(void*)){
	if(treeRoot == NULL)
		return;

	struct bstNode* root = (struct bstNode*) treeRoot; 
	bstInorderTraversal(root->left, f);
	bstInorderTraversal(root->right, f);	
	f(treeRoot);
}

// Search for key in bst. Returns NULL if not found. O(log n)
void* bstSearch(char* key, unsigned int keySize, void* treeRoot){
	struct bstNode* root = (struct bstNode*) treeRoot;
	int cmpRes;

	while(root != NULL){
		if(!(cmpRes = memcmp(key, root->key, min(keySize, root->keySize))))
			break;

		if(cmpRes < 0)
			root = root->left;
		else
			root = root->right;
	}

	return (void*)root;
}

void bstRotateLeft(struct bstNode **root, struct bstNode *x){
	struct bstNode *y = x->right;
	x->right = y->left;

	if(y->left != NULL)
		y->left->parent = x;

	y->parent = x->parent;

	if(x->parent == NULL)
		*root = y;
	else if(x == x->parent->left)
		x->parent->left = y;
	else
		x->parent->right = y;

	y->left = x;
	x->parent = y;
}

void bstRotateRight(struct bstNode **root, struct bstNode *x){
	struct bstNode *y = x->left;
	x->left = y->right;

	if(y->right != NULL)
		y->right->parent = x;

	y->parent = x->parent;

	if(x->parent == NULL)
		*root = y;
	else if(x == x->parent->left)
		x->parent->left = y;
	else
		x->parent->right = y;

	y->right = x;
	x->parent = y;
}

// Assures log n height of tree after insertion
void bstCleanupInsert(struct bstNode **root, struct bstNode *x){
	struct bstNode *uncle;

	if(x->parent != NULL && x->parent->parent != NULL)
		while(x->parent->color == bstRed){
			if(x->parent == x->parent->parent->left){
				uncle = x->parent->parent->right;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					x->parent->color = bstBlack;
					uncle->color = bstBlack;
					x->parent->parent->color = bstRed;
					x = x->parent->parent;
				}else{
					if(x == x->parent->right){
						x = x->parent;
						bstRotateLeft(root, x);
					}

					x->parent->color = bstBlack;
					x->parent->parent->color = bstRed;
					bstRotateRight(root, x->parent->parent);
				}
			}else{
				uncle = x->parent->parent->left;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					x->parent->color = bstBlack;
					uncle->color = bstBlack;
					x->parent->parent->color = bstRed;
					x = x->parent->parent;
				}else{
					if(x == x->parent->left){
						x = x->parent;
						bstRotateRight(root, x);
					}

					x->parent->color = bstBlack;
					x->parent->parent->color = bstRed;
					bstRotateLeft(root, x->parent->parent);
				}
			}
		}

	(*root)->color = bstBlack;
}

// RB-BST insert. O(log n) Inserted nodes will be painted red. Does not allow duplicates
void bstInsert(void* newNode, void** treeRoot){
	int cmpRes;
	struct bstNode* new_node = (struct bstNode*)newNode, *parent = NULL, *x;
	struct bstNode** root = (struct bstNode**)treeRoot;

	x = *root;
	while(x != NULL){
		parent = x;

		if(!(cmpRes = memcmp(new_node->key, x->key, min(new_node->keySize, x->keySize))))
			return; // This stops duplicates

		if(cmpRes < 0)
			x = x->left;
		else
			x = x->right;
	}

	new_node->parent = parent;
	new_node->color = bstRed;
	new_node->left = NULL;
	new_node->right = NULL;

	if(parent == NULL)
		*treeRoot = new_node;

	else if(memcmp(new_node->key, parent->key, min(new_node->keySize, parent->keySize)) < 0)
		parent->left = new_node;
	else
		parent->right = new_node;

	if(parent != NULL)
		bstCleanupInsert(root, new_node);
}

// Find bstNode with minimum key O(log n)
void* bstMinimum(void* x){
	struct bstNode* n = (struct bstNode*) x;

	while(n->left != NULL)
		n = n->left;

	return (void*) n;
}

void* bstFirst(void* x){
	return bstMinimum(x);
}

// Find bstNode with maximum key O(log n)
void* bstMaximum(void* x){
	struct bstNode* n = (struct bstNode*) x;

	while(n->right != NULL)
		n = n->right;

	return (void*) n;
}

void* bstLast(void* x){
	return bstMaximum(x);
}

// Gets the inorder successor; O(log n). For k succesive calls: O(log(n) + k)
void* bstSuccessor(void* x){
	struct bstNode* n = (struct bstNode*) x;

	if(n->right != NULL)
		return bstMinimum(n->right);

	struct bstNode *y = n->parent;
	while(y != NULL && n == y->right){
		n = y;
		y = y->parent;
	}

	return (void*) y;
}

void* bstNext(void* x){
	return bstSuccessor(x);
}

// Gets the inorder predecessor; O(log n). For k succesive calls: O(log(n) + k)
void* bstPredecessor(void* x){
	struct bstNode* n = (struct bstNode*) x;

	if(n->left != NULL)
		return bstMaximum(n->left);

	struct bstNode *y = n->parent;
	while(y != NULL && n == y->left){
		n = y;
		y = y->parent;
	}

	return (void*) y;
}

void* bstPrevious(void* x){
	return bstPredecessor(x);
}

// Assures log n height after removal
void bstCleanupRemove(struct bstNode **root, struct bstNode *x){
	struct bstNode *uncle;

	if(x->parent != NULL && x->parent->parent != NULL)
		while(x != *root && x->color == bstBlack){
			if(x == x->parent->left){
				uncle = x->parent->right;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					uncle->color = bstBlack;
					x->parent->color = bstRed;
					bstRotateLeft(root, x->parent);
					uncle = x->parent->right;
				}

				if(uncle->left->color == bstBlack && uncle->right->color == bstBlack){
					uncle->color = bstRed;
					x = x->parent;
				}else{
					if(uncle->right->color == bstBlack){
						uncle->left->color = bstBlack;
						uncle->color = bstRed;
						bstRotateRight(root, uncle);
						uncle = x->parent->right;
					}

					uncle->color = x->parent->color;
					x->parent->color = bstBlack;
					uncle->right->color = bstBlack;
					bstRotateLeft(root, x->parent);
					x = *root;
				}
			}else{
				uncle = x->parent->left;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					uncle->color = bstBlack;
					x->parent->color = bstRed;
					bstRotateRight(root, x->parent);
					uncle = x->parent->left;
				}

				if(uncle->left->color == bstBlack && uncle->right->color == bstBlack){
					uncle->color = bstRed;
					x = x->parent;
				}else{
					if(uncle->left->color == bstBlack){
						uncle->right->color = bstBlack;
						uncle->color = bstRed;
						bstRotateLeft(root, uncle);
						uncle = x->parent->left;
					}

					uncle->color = x->parent->color;
					x->parent->color = bstBlack;
					uncle->left->color = bstBlack;
					bstRotateRight(root, x->parent);
					x = *root;
				}
			}
		}

	x->color = bstBlack;
}

void bstTransplant(struct bstNode** root, struct bstNode* u, struct bstNode* v){
	if(u->parent == NULL)
		*root = v;
	else if(u == u->parent->left)
		u->parent->left = v;
	else
		u->parent->right = v;

	v->parent = u->parent;
}

// RB-Bst remove, O(log n)
void bstRemoveNode(void* xx, void** treeRoot){
	if(xx == NULL)
		return;

	struct bstNode *x, *y, *z = (struct bstNode*) xx; 
	struct bstNode **root = (struct bstNode**)treeRoot;

	y = z;
	enum rbColors yOrgColor = y->color;

	if(z->left == NULL){
		x = z->right;
		bstTransplant(root, z, z->right);
	}else if(z->right == NULL){
		x = z->left;
		bstTransplant(root, z, z->left);
	}else{
		y = bstMinimum(z->right);
		yOrgColor = y->color;
		x = y->right;

		if(y->parent == z)
			x->parent = y;
		else{
			bstTransplant(root, y, y->right);
			y->right = z->right;
			y->right->parent = y;
		}

		bstTransplant(root, z, y);
		y->left = z->left;
		y->left->parent = y;
		y->color = z->color;
	}

	if(yOrgColor == bstBlack)
		bstCleanupRemove(root, x);

	//bstFreeNode(z); 
}
