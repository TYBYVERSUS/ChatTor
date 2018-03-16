// Various Binary Search Trees and functions to modify/navigate them

enum rbColors {bstRed, bstBlack};

struct bstNode {
	char *index;
	unsigned int index_length; // Includes the terminating NULL byte
	enum rbColors color;
	struct bstNode *left, *right, *parent;
};

struct roomIdentityBST;
struct socketIdentityBST;
struct socketBST;
struct roomBST;
struct identityNode;

// Global trees
struct socketBST *socketsRoot = NULL;
struct roomBST *roomsRoot = NULL;

// This implementation can still be improved... Can probably do without a parent pointer and there are simpler ways to iterate through each node
// This still has bugs that will crash with more than a couple of users (probably whent he tree tries to rebalance). I will fix this next time.

// Useful functions:
// Search for key in bst. Returns NULL if not found. O(log n)
//void* bstSearch(char* key, unsigned int keySize, void* root);

// Properly mallocs a new bst-node. Use this before bstInsert
/*struct roomIdentityBST* bstMakeRoomIdentityBST(char* key, unsigned int keySize, struct identityNode *identity);
struct socketIdentityBST* bstMakeSocketIdentityBST(char* key, unsigned int keySize, struct identityNode *identity);
struct roomBST* bstMakeRoomBST(char* key, unsigned int keySize, struct roomIdentityBST* identities);
struct socketBST* bstMakeSocketBST(char* key, unsigned int keySize, struct socketIdentityBST* identities, int fd);
*/
// RB-BST insert. O(log n) Inserted nodes will be painted red. Does not allow duplicates
// Use a proper make function first on newNode
//void bstInsert(void* newNode, void** treeRoot);

// Removes and properly frees removed node based on the key and the keysize
/*void bstRemoveRoomIdentityBST(char *key, unsigned int keySize, struct roomIdentityBST** root);
void bstRemoveSocketIdentityBST(char *key, unsigned int keySize, struct socketIdentityBST** root);
void bstRemoveSocketBST(char *key, unsigned int keySize, struct socketBST** root);
void bstRemoveRoomBST(char *key, unsigned int keySize, struct roomBST** root);

// RB-Bst remove, O(log n). NOTE: does not free the deleted node. Call bstFreeNode for that
void bstRemoveNode(void* x, void** root);
*/
// Calls f on each node in sorted ascending order by their keys. O(n)
/*void bstInorderTraversal(void* root, void (*f)(void*));
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
void* bstLast(void* x);*/


struct socketBST{
	char *index;
	unsigned int index_length;
	enum rbColors color;
	struct socketBST *left, *right, *parent;
	int fd;

	struct socketIdentityBST *identities;
	pthread_mutex_t identities_mutex;
};
struct roomBST{
	char *index;
	unsigned int index_length;
	enum rbColors color;
	struct roomBST *left, *right, *parent;

	struct roomIdentityBST *first, *last, *identities;
	pthread_mutex_t identities_mutex;
};

struct socketIdentityBST{
	char *index;
	unsigned int index_length;
	enum rbColors color;
	struct socketIdentityBST *left, *right, *parent;
	struct identityNode *identity;
};
struct roomIdentityBST{
	char *index;
	unsigned int index_length;
	enum rbColors color;
	struct roomIdentityBST *left, *right, *parent;
	struct identityNode *identity;

	struct roomIdentityBST *next;
};

struct identityNode{
	char *color, *name, *trip;
	struct socketBST *socket_node;
	struct roomBST *room_node;
};



// Make sure stuff gets freed properly when removed
/*void bstFreeBstNode(void* x){
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
*/


// --------- Below is the red black implementation. Venture down on your own peril. --------- //

// Calls f on each node in sorted ascending order by their keys. O(n)
/*void bstInorderTraversal(void* treeRoot, void (*f)(void*)){
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
*/
// Search for key in bst. Returns NULL if not found. O(log n)
void* bstSearch(void* index, unsigned int index_length, void* tree_root){
	struct bstNode* root = (struct bstNode*)tree_root;
	int cmpRes;

	while(root != NULL){
		if(!(cmpRes = memcmp(index, root->index, min(index_length, root->index_length))))
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

// RB-BST insert. O(log n). Inserted nodes will be painted red. Does not allow duplicates
void bstInsert(void* new_node, void** tree_root){
	int cmp_result;
	struct bstNode *node = new_node, *uncle, **root = (struct bstNode**)tree_root;

	node->parent = *root;
	node->left = NULL;
	node->right = NULL;

	while(node->parent != NULL){
		if(!(cmp_result = memcmp(node->index, node->parent->index, min(node->index_length, node->parent->index_length))))
			return;

		if(cmp_result < 0){
			if(node->parent->left == NULL){
				node->parent->left = node;
				break;
			}

			node->parent = node->parent->left;
		}else{
			if(node->parent->right == NULL){
				node->parent->right = node;
				break;
			}

			node->parent = node->parent->right;
		}
	}

	if(node->parent == NULL){
		node->color = bstBlack;
		*tree_root = node;
		return;
	}

	node->color = bstRed;

	// Cleanup to ensure n height
	if(node->parent != NULL && node->parent->parent != NULL)
		while(node->parent->color == bstRed){
			if(node->parent == node->parent->parent->left){
				uncle = node->parent->parent->right;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					node->parent->color = bstBlack;
					uncle->color = bstBlack;
					node->parent->parent->color = bstRed;
					node = node->parent->parent;
				}else{
					if(node == node->parent->right){
						node = node->parent;
						bstRotateLeft(root, node);
					}

					node->parent->color = bstBlack;
					node->parent->parent->color = bstRed;
					bstRotateRight(root, node->parent->parent);
				}
			}else{
				uncle = node->parent->parent->left;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					node->parent->color = bstBlack;
					uncle->color = bstBlack;
					node->parent->parent->color = bstRed;
					node = node->parent->parent;
				}else{
					if(node == node->parent->left){
						node = node->parent;
						bstRotateRight(root, node);
					}

					node->parent->color = bstBlack;
					node->parent->parent->color = bstRed;
					bstRotateLeft(root, node->parent->parent);
				}
			}
		}

	(*root)->color = bstBlack;
}

// Find bstNode with minimum key O(log n)
/*void* bstMinimum(void* x){
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
*/
// Gets the inorder successor; O(log n). For k succesive calls: O(log(n) + k)
void* bstSuccessor(void* x){
	struct bstNode* n = (struct bstNode*) x;

	if(n->right != NULL){
		n = n->right;
		while(n->left != NULL)
			n = n->left;

		return (void*)n;
	}

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
/*
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
*/

void bstTransplant(struct bstNode** root, struct bstNode* replace, struct bstNode* with){
	if(replace->parent == NULL)
		*root = with;
	else if(replace == replace->parent->left)
		replace->parent->left = with;
	else
		replace->parent->right = with;

	if(with != NULL)
		with->parent = replace->parent;
}

// RB-Bst remove, O(log n)
void bstRemoveNode(void* kill, void** tree_root){
	if(kill == NULL)
		return;

	struct bstNode *new, *old = (struct bstNode*)kill;
	struct bstNode **root = (struct bstNode**)tree_root;

	enum rbColors old_color = old->color;

	if(old->left == NULL){
		new = old->right;
		bstTransplant(root, old, new);
	}else if(old->right == NULL){
		new = old->left;
		bstTransplant(root, old, new);
	}else{
		struct bstNode* tmp = old->right;
		while(tmp->left != NULL)
			tmp = tmp->left;

		old_color = tmp->color;
		new = tmp->right;

		if(tmp->parent == old)
			new->parent = tmp;
		else{
			bstTransplant(root, tmp, tmp->right);
			tmp->right = old->right;
			tmp->right->parent = tmp;
		}

		bstTransplant(root, old, tmp);
		tmp->left = old->left;
		tmp->left->parent = tmp;
		tmp->color = old->color;
	}

	if(old_color == bstRed || new == NULL)
		return;

	// Cleanup to ensure n height
	if(new->parent != NULL && new->parent->parent != NULL){
		struct bstNode *uncle;

		while(new != *root && new->color == bstBlack){
			if(new == new->parent->left){
				uncle = new->parent->right;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					uncle->color = bstBlack;
					new->parent->color = bstRed;
					bstRotateLeft(root, new->parent);
					uncle = new->parent->right;
				}

				if(uncle->left->color == bstBlack && uncle->right->color == bstBlack){
					uncle->color = bstRed;
					new = new->parent;
				}else{
					if(uncle->right->color == bstBlack){
						uncle->left->color = bstBlack;
						uncle->color = bstRed;
						bstRotateRight(root, uncle);
						uncle = new->parent->right;
					}

					uncle->color = new->parent->color;
					new->parent->color = bstBlack;
					uncle->right->color = bstBlack;
					bstRotateLeft(root, new->parent);
					new = *root;
				}
			}else{
				uncle = new->parent->left;
				if(uncle == NULL)
					return;

				if(uncle->color == bstRed){
					uncle->color = bstBlack;
					new->parent->color = bstRed;
					bstRotateRight(root, new->parent);
					uncle = new->parent->left;
				}

				if(uncle->left->color == bstBlack && uncle->right->color == bstBlack){
					uncle->color = bstRed;
					new = new->parent;
				}else{
					if(uncle->left->color == bstBlack){
						uncle->right->color = bstBlack;
						uncle->color = bstRed;
						bstRotateLeft(root, uncle);
						uncle = new->parent->left;
					}

					uncle->color = new->parent->color;
					new->parent->color = bstBlack;
					uncle->left->color = bstBlack;
					bstRotateRight(root, new->parent);
					new = *root;
				}
			}
		}
	}

	new->color = bstBlack;
}
