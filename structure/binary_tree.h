#ifndef __STEPHEN_BINARY_TREE_H__
#define __STEPHEN_BINARY_TREE_H__

typedef int KEY_VALUE;

#define BSTREE_ENTRY(name, type)	\
		struct name {				\
        	struct type* left;		\
        	struct type* right;		\
		}

typedef struct BSTREE_NODE {
	BSTREE_ENTRY(, BSTREE_NODE) bst;
	KEY_VALUE key;
	void* pValue;
} bstree_node_t;

typedef struct BSTREE {
	struct BSTREE_NODE* root;
} bstree_t;

bstree_node_t* CreateBstreeNode(KEY_VALUE key);
bool InsertBstree(bstree_t * tree, KEY_VALUE key);
int TraversalBstree(bstree_node_t* node);


#endif//__STEPHEN_BINARY_TREE_H__