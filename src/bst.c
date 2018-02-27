// Various Binary Search Trees and functions to modify/navigate them

#define bstRED 1
#define bstBLACK 0
struct bstNode {
	char *key;
	unsigned int keySize; // Includes the terminating NULL byte
	unsigned char color;
	struct bstNode *left, *right, *parent;
} bstNIL = {NULL, 0, bstBLACK, NULL, NULL, NULL};

// Is this useful?
// #define something &bstNIL

struct roomIdentityBST;
struct socketIdentityBST;
struct socketBST;
struct roomBST;
struct identityNode;

// GLOBAL TREES
// New trees should be set to &bstNIL. &bstNIL is the identity for the trees
struct socketBST *sockets_root = (struct socketBST *) &bstNIL;
struct roomBST *rooms_root = (struct roomBST *) &bstNIL;




// Useful functions:
// Search for key in bst. Returns &bstNIL if not found. O(log n)
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


struct roomIdentityBST{
	char *key;
	unsigned int keySize;
	unsigned char color;
	struct bstNode *left, *right, *parent;

	struct identityNode *identity;
};

struct socketIdentityBST{
	char *key;
	unsigned int keySize;
	unsigned char color;
	struct bstNode *left, *right, *parent;

	struct identityNode *identity;
};

struct socketBST{
	char *key;
	unsigned int keySize;
	unsigned char color;
	struct bstNode *left, *right, *parent;

	int fd;
	struct socketIdentityBST *identities;
	pthread_mutex_t identities_mutex;
};

struct roomBST{
	char *key;
	unsigned int keySize;
	unsigned char color;
	struct bstNode *left, *right, *parent;

	struct roomIdentityBST *identities;
	pthread_mutex_t identities_mutex;
};

struct identityNode{
	char *color, *name, trip[21];
	struct socketBST *socketBST; // same name as the struct, hmmm...
	struct roomBST *roomBST;
};



// Make sure stuff gets freed properly when removed
void bstFreeBstNode(void* x){
	if(x == NULL || x == &bstNIL) return;
	struct bstNode* n = (struct bstNode*) x;
	free(n->key);
	free(n);
}
void bstFreeRoomSocketIdentityNode(void* x){
	if(x == NULL) return;
	struct roomIdentityBST* n = (struct roomIdentityBST*) x;
	free(n->identity->color);
	free(n->identity->name);
	free(n->identity);
}
void bstRemoveRoomIdentityBST(char *key, unsigned int keySize, struct roomIdentityBST** root){
	void* x = bstSearch(key, keySize, (void*) *root);
	if((struct bstNode*) x == &bstNIL || x == NULL) return;
	bstRemoveNode(x, (void**) root);
	bstFreeRoomSocketIdentityNode(x);
	bstFreeBstNode(x);
}
void bstRemoveSocketIdentityBST(char *key, unsigned int keySize, struct socketIdentityBST** root){
	void* x = bstSearch(key, keySize, (void*) *root);
	if((struct bstNode*) x == &bstNIL || x == NULL) return;
	bstRemoveNode(x, (void**) root);
	bstFreeRoomSocketIdentityNode(x);
	bstFreeBstNode(x);
}

void bstRemoveSocketBST(char *key, unsigned int keySize, struct socketBST** root){
	struct socketBST* x = (struct socketBST*) bstSearch(key, keySize, (void*) *root);
	if((struct bstNode*) x == &bstNIL || x == NULL) return;
	bstRemoveNode((void*) x, (void**) root);
	// Must be postorder, not inorder or foreach
	bstPostOrderTraversal(x->identities, bstFreeRoomSocketIdentityNode);
	bstPostOrderTraversal(x->identities, bstFreeBstNode);
	bstFreeBstNode(x);
}
void bstRemoveRoomBST(char *key, unsigned int keySize, struct roomBST** root){
	struct roomBST* x = (struct roomBST*) bstSearch(key, keySize, (void*) *root);
	if((struct bstNode*) x == &bstNIL || x == NULL) return;
	bstRemoveNode((void*) x, (void**) root);
	// Must be postorder, not inorder or foreach
	bstPostOrderTraversal(x->identities, bstFreeRoomSocketIdentityNode);
	bstPostOrderTraversal(x->identities, bstFreeBstNode);
	bstFreeBstNode(x);
}

// Make sure things are malloced solely for the tree so that they aren't freed randomly
struct roomIdentityBST* bstMakeRoomIdentityBST(char* key, unsigned int keySize, struct identityNode *identity){
	struct roomIdentityBST* newNode = malloc(sizeof(struct roomIdentityBST));
	newNode->keySize = keySize;
	newNode->identity = identity;
	newNode->key = malloc(keySize);
	memcpy(newNode->key, key, keySize);
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

struct roomBST* bstMakeRoomBST(char* key, unsigned int keySize, struct roomIdentityBST* identities){
	struct roomBST* newNode = malloc(sizeof(struct roomBST));
	newNode->keySize = keySize;
	newNode->identities = identities;
	pthread_mutex_init(&newNode->identities_mutex, NULL);
	newNode->key = malloc(keySize);
	memcpy(newNode->key, key, keySize);
	return newNode;
}
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


unsigned int max(unsigned int a, unsigned int b){
	return a < b ? b : a;
}

// --------- Below is the red black implementation. Venture down on your own peril. --------- //

// Calls f on each node in sorted ascending order by their keys. O(n)
void bstInorderTraversal(void* treeRoot, void (*f)(void*)){
	struct bstNode* root = (struct bstNode*) treeRoot; 
	if(root == &bstNIL) return;
	bstInorderTraversal(root->left, f);
	f(treeRoot);
	bstInorderTraversal(root->right, f);	
}
void bstForEach(void* root, void (*f)(void*)){
	bstInorderTraversal(root, f);
}

void bstPostOrderTraversal(void* treeRoot, void (*f)(void*)){
	struct bstNode* root = (struct bstNode*) treeRoot; 
	if(root == &bstNIL) return;
	bstInorderTraversal(root->left, f);
	bstInorderTraversal(root->right, f);	
	f(treeRoot);
}

// Search for key in bst. Returns &bstNIL if not found. O(log n)
void* bstSearch(char* key, unsigned int keySize, void* treeRoot){
	struct bstNode* root = (struct bstNode*) treeRoot;

	while(root != &bstNIL){
		int cmpRes = memcmp(key, root->key, max(keySize, root->keySize));
		if(cmpRes == 0) return root;
		if(cmpRes < 0) root = root->left;
		else root = root->right;
	}
	return (void*)root;
}

void bstRotateLeft(struct bstNode **root, struct bstNode *x){
	struct bstNode *y = x->right;
	x->right = y->left;
	if(y->left != &bstNIL) y->left->parent = x;
	y->parent = x->parent;
	if(x->parent == &bstNIL) *root = y;
	else if(x == x->parent->left) x->parent->left = y;
	else x->parent->right = y;
	y->left = x;
	x->parent = y;
}
void bstRotateRight(struct bstNode **root, struct bstNode *x){
	struct bstNode *y = x->left;
	x->left = y->right;
	if(y->right != &bstNIL) y->right->parent = x;
	y->parent = x->parent;
	if(x->parent == &bstNIL) *root = y;
	else if(x == x->parent->left) x->parent->left = y;
	else x->parent->right = y;
	y->right = x;
	x->parent = y;
}

// Assures log n height of tree after insertion
void bstCleanupInsert(struct bstNode **root, struct bstNode *x){
	while(x->parent->color == bstRED){
		if(x->parent == x->parent->parent->left){
			struct bstNode *uncle = x->parent->parent->right;
			if(uncle->color == bstRED){
				x->parent->color = bstBLACK;
				uncle->color = bstBLACK;
				x->parent->parent->color = bstRED;
				x = x->parent->parent;
			} else {
				if(x == x->parent->right){
					x = x->parent;
					bstRotateLeft(root, x);
				}
				x->parent->color = bstBLACK;
				x->parent->parent->color = bstRED;
				bstRotateRight(root, x->parent->parent);
			}
		} else {
			struct bstNode *uncle = x->parent->parent->left;
			if(uncle->color == bstRED){
				x->parent->color = bstBLACK;
				uncle->color = bstBLACK;
				x->parent->parent->color = bstRED;
				x = x->parent->parent;
			} else {
				if(x == x->parent->left){
					x = x->parent;
					bstRotateRight(root, x);
				}
				x->parent->color = bstBLACK;
				x->parent->parent->color = bstRED;
				bstRotateLeft(root, x->parent->parent);
			}
		}
	}
	(*root)->color = bstBLACK;
}

// RB-BST insert. O(log n) Inserted nodes will be painted red. Does not allow duplicates
void bstInsert(void* newNode, void** treeRoot){
	struct bstNode* new_node = (struct bstNode*) newNode;
	struct bstNode** root = (struct bstNode**) treeRoot;

	struct bstNode *parent = &bstNIL, *x = *root;
	while(x != &bstNIL){
		parent = x;
		int cmpRes = memcmp(new_node->key, x->key, max(new_node->keySize, x->keySize));
		if(cmpRes == 0) return; // this line stops duplicates
		if(cmpRes < 0) x = x->left;
		else x = x->right;
	}

	new_node->parent = parent;
	new_node->color = bstRED;
	new_node->left = &bstNIL;
	new_node->right = &bstNIL;
	if(parent == &bstNIL){
		*treeRoot = new_node;
	}
	else if(memcmp(new_node->key, parent->key, max(new_node->keySize, parent->keySize)) < 0){
		parent->left = new_node;
	} else parent->right = new_node;
	bstCleanupInsert(root, new_node);
}

// Find bstNode with minimum key O(log n)
void* bstMinimum(void* x){
	struct bstNode* n = (struct bstNode*) x;
	while(n->left != &bstNIL) n = n->left;
	return (void*) n;
}
void* bstFirst(void* x){
	return bstMinimum(x);
}

// Find bstNode with maximum key O(log n)
void* bstMaximum(void* x){
	struct bstNode* n = (struct bstNode*) x;
	while(n->right != &bstNIL) n = n->right;
	return (void*) n;
}
void* bstLast(void* x){
	return bstMaximum(x);
}

// Gets the inorder successor; O(log n). For k succesive calls: O(log(n) + k)
void* bstSuccessor(void* x){
	struct bstNode* n = (struct bstNode*) x;
	if(n->right != &bstNIL) return bstMinimum(n->right);
	struct bstNode *y = n->parent;
	while(y != &bstNIL && n == y->right){
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
	if(n->left != &bstNIL) return bstMaximum(n->left);
	struct bstNode *y = n->parent;
	while(y != &bstNIL && n == y->left){
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
	while(x != *root && x->color == bstBLACK){
		if(x == x->parent->left){
			struct bstNode *uncle = x->parent->right;
			if(uncle->color == bstRED){
				uncle->color = bstBLACK;
				x->parent->color = bstRED;
				bstRotateLeft(root, x->parent);
				uncle = x->parent->right;
			}
			if(uncle->left->color == bstBLACK && uncle->right->color == bstBLACK){
				uncle->color = bstRED;
				x = x->parent;
			} else {
				if(uncle->right->color == bstBLACK){
					uncle->left->color = bstBLACK;
					uncle->color = bstRED;
					bstRotateRight(root, uncle);
					uncle = x->parent->right;
				}
				uncle->color = x->parent->color;
				x->parent->color = bstBLACK;
				uncle->right->color = bstBLACK;
				bstRotateLeft(root, x->parent);
				x = *root;
			}
		} else {
			struct bstNode *uncle = x->parent->left;
			if(uncle->color == bstRED){
				uncle->color = bstBLACK;
				x->parent->color = bstRED;
				bstRotateRight(root, x->parent);
				uncle = x->parent->left;
			}
			if(uncle->left->color == bstBLACK && uncle->right->color == bstBLACK){
				uncle->color = bstRED;
				x = x->parent;
			} else {
				if(uncle->left->color == bstBLACK){
					uncle->right->color = bstBLACK;
					uncle->color = bstRED;
					bstRotateLeft(root, uncle);
					uncle = x->parent->left;
				}
				uncle->color = x->parent->color;
				x->parent->color = bstBLACK;
				uncle->left->color = bstBLACK;
				bstRotateRight(root, x->parent);
				x = *root;
			}
		}
	}
	x->color = bstBLACK;
}

void bstTransplant(struct bstNode** root, struct bstNode* u, struct bstNode* v){
	if(u->parent == &bstNIL) *root = v;
	else if(u == u->parent->left) u->parent->left = v;
	else u->parent->right = v;
	v->parent = u->parent;
}

// RB-Bst remove, O(log n)
void bstRemoveNode(void* xx, void** treeRoot){
	if(xx == &bstNIL) return;
	struct bstNode* z = (struct bstNode*) xx; 

	struct bstNode** root = (struct bstNode**) treeRoot;

	struct bstNode *y = z, *x;
	unsigned char yOrgColor = y->color;
	if(z->left == &bstNIL){
		x = z->right;
		bstTransplant(root, z, z->right);
	} else if(z->right == &bstNIL){
		x = z->left;
		bstTransplant(root, z, z->left);
	} else {
		y = bstMinimum(z->right);
		yOrgColor = y->color;
		x = y->right;
		if(y->parent == z)
			x->parent = y;
		else {
			bstTransplant(root, y, y->right);
			y->right = z->right;
			y->right->parent = y;
		}
		bstTransplant(root, z, y);
		y->left = z->left;
		y->left->parent = y;
		y->color = z->color;
	}
	if(yOrgColor == bstBLACK) bstCleanupRemove(root, x);
	//bstFreeNode(z); 
}
