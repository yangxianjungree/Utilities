#include <cstdlib>
#include <cstdio>

#include "binary_tree.h"


bstree_node_t* CreateBstreeNode(KEY_VALUE key)
{
	bstree_node_t *node = (bstree_node_t *)malloc(sizeof(bstree_node_t));
	if (nullptr == node) {
		return nullptr;
	}

	node->key = key;
	node->bst.left = node->bst.right = nullptr;

	return node;
}

bool InsertBstree(bstree_t * tree, KEY_VALUE key)
{
	if (nullptr == tree) {
		return false;
	}

	if (tree->root == nullptr) {
		tree->root = CreateBstreeNode(key);
		return true;
	}

	bstree_node_t *node = tree->root;
	bstree_node_t *tmp  = tree->root;

	while (node != nullptr) {
		tmp = node;

		if (key < node->key) {
			node = node->bst.left;
		} else if (key > node->key) {
			node = node->bst.right;
		} else {
			return true;
		}
	}

	if (key < tmp->key) {
		tmp->bst.left = CreateBstreeNode(key);
	} else {
		tmp->bst.right = CreateBstreeNode(key);
	}

	return true;
}

int TraversalBstree(bstree_node_t* node)
{
	if (node == nullptr) {
		return 0;
	}

	TraversalBstree(node->bst.left);
	printf("%4d", node->key);
	TraversalBstree(node->bst.right);

	return 0;
}