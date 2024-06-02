/*
 * Copyright (C) Igor Sysoev
 */
#ifndef RBTREE_H
#define RBTREE_H

typedef struct rbtree_node {
    struct rbtree_node *left;
    struct rbtree_node *right;
    struct rbtree_node *parent;
    uint8_t color;
} rbtree_node;

struct rbtree {
    rbtree_node sentinel;
};

/*
 * A comparison function should return intptr_t result because
 * this eliminates overhead required to implement correct addresses
 * comparison without result truncation.
 */
typedef intptr_t (*rbtree_compare_t)(rbtree_node *, rbtree_node *);
void rbtree_init(struct rbtree *, rbtree_compare_t);
void rbtree_insert(struct rbtree *, rbtree_node *);
void rbtree_delete(struct rbtree *, rbtree_node *);
/*
 * rbtree_destroy_next() is iterator to use only while rbtree destruction.
 * It deletes a node from rbtree and returns the node.  The rbtree is not
 * rebalanced after deletion.  At the beginning the "next" parameter should
 * be equal to rbtree root.  The iterator should be called in loop until
 * the "next" parameter will be equal to the rbtree sentinel.  No other
 * operations must be performed on the rbtree while destruction.
 */
rbtree_node *rbtree_destroy_next(struct rbtree *, rbtree_node **);

#define rbtree_root(tree)                                                     \
    ((tree)->sentinel.left)

#define rbtree_sentinel(tree)                                                 \
    (&(tree)->sentinel)

#define rbtree_is_empty(tree)                                                 \
    (rbtree_root(tree) == rbtree_sentinel(tree))

#define rbtree_min(tree)                                                      \
    rbtree_branch_min(tree, &(tree)->sentinel)

#define rbtree_is_there_successor(tree, node)                                 \
    ((node) != rbtree_sentinel(tree))

static inline rbtree_node *
rbtree_branch_min(struct rbtree *tree, rbtree_node *node)
{
    while (node->left != rbtree_sentinel(tree)) {
        node = node->left;
    }

    return node;
}

static inline rbtree_node *
rbtree_node_successor(struct rbtree *tree, rbtree_node *node)
{
    rbtree_node *parent;

    if (node->right != rbtree_sentinel(tree)) {
        return rbtree_branch_min(tree, node->right);
    }

    for ( ;; ) {
        parent = node->parent;

        /*
         * Explicit test for a root node is not required here, because
         * the root node is always the left child of the sentinel.
         */
        if (node == parent->left) {
            return parent;
        }

        node = parent;
    }
}

#endif /* RBTREE_H */
