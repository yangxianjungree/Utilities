#include <cstdio>

#include "binary_tree.h"
#include "red_black_tree.h"

#define ARRAY_LENGTH		15
int keys[ARRAY_LENGTH] 		= {25, 67, 89, 90, 15,  78, 56, 69, 16, 26,  47, 10, 29, 61, 63};

int main()
{
	// for binary tree.
	bstree_t tree = {0};
	for (int i = 0; i < ARRAY_LENGTH; ++i) {
		InsertBstree(&tree, keys[i]);
	}
	TraversalBstree(tree.root);
	printf("\n");



	return 0;
}