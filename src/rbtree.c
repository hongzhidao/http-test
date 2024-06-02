/*
 * Copyright (C) Igor Sysoev
 */
#include "headers.h"

/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */
static void rbtree_insert_fixup(rbtree_node *);
static void rbtree_delete_fixup(struct rbtree *, rbtree_node *);
static inline void rbtree_left_rotate(rbtree_node *);
static inline void rbtree_right_rotate(rbtree_node *);
static inline void rbtree_parent_relink(rbtree_node *, rbtree_node *);

#define RBTREE_BLACK  0
#define RBTREE_RED    1

#define rbtree_comparison_callback(tree)                                    \
    ((rbtree_compare_t) (tree)->sentinel.right)


void
rbtree_init(struct rbtree *tree, rbtree_compare_t compare)
{
    /*
     * The sentinel is used as a leaf node sentinel and as a tree root
     * sentinel: it is a parent of a root node and the root node is
     * the left child of the sentinel.  Combining two sentinels in one
     * entry and the fact that the sentinel's left child is a root node
     * simplifies rbtree_node_successor() and eliminates explicit
     * root node test before or inside rbtree_min().
     */

    /* The root is empty. */
    tree->sentinel.left = &tree->sentinel;

    /*
     * The sentinel's right child is never used so
     * comparison callback can be safely stored here.
     */
    tree->sentinel.right = (void *) compare;

    /* The root and leaf sentinel must be black. */
    tree->sentinel.color = RBTREE_BLACK;
}


void
rbtree_insert(struct rbtree *tree, rbtree_node *new_node)
{
    rbtree_node *node, *sentinel, **child;
    rbtree_compare_t  compare;

    node = rbtree_root(tree);
    sentinel = rbtree_sentinel(tree);

    new_node->left = sentinel;
    new_node->right = sentinel;
    new_node->color = RBTREE_RED;

    compare = (rbtree_compare_t) tree->sentinel.right;
    child = &rbtree_root(tree);

    while (*child != sentinel) {
        node = *child;
        child = (compare(new_node, node) < 0) ? &node->left : &node->right;
    }

    *child = new_node;
    new_node->parent = node;

    rbtree_insert_fixup(new_node);

    node = rbtree_root(tree);
    node->color = RBTREE_BLACK;
}


static void
rbtree_insert_fixup(rbtree_node *node)
{
    rbtree_node *parent, *grandparent, *uncle;

    /*
     * Prefetching parent nodes does not help here because they are
     * already traversed during insertion.
     */

    for ( ;; ) {
        parent = node->parent;

        /*
         * Testing whether a node is a tree root is not required here since
         * a root node's parent is the sentinel and it is always black.
         */
        if (parent->color == RBTREE_BLACK) {
            return;
        }

        grandparent = parent->parent;

        if (parent == grandparent->left) {
            uncle = grandparent->right;

            if (uncle->color == RBTREE_BLACK) {

                if (node == parent->right) {
                    node = parent;
                    rbtree_left_rotate(node);
                }

                /*
                 * rbtree_left_rotate() swaps parent and
                 * child whilst keeps grandparent the same.
                 */
                parent = node->parent;

                parent->color = RBTREE_BLACK;
                grandparent->color = RBTREE_RED;

                rbtree_right_rotate(grandparent);
                /*
                 * rbtree_right_rotate() does not change node->parent
                 * color which is now black, so testing color is not required
                 * to return from function.
                 */
                return;
            }

        } else {
            uncle = grandparent->left;

            if (uncle->color == RBTREE_BLACK) {

                if (node == parent->left) {
                    node = parent;
                    rbtree_right_rotate(node);
                }

                /* See the comment in the symmetric branch above. */
                parent = node->parent;

                parent->color = RBTREE_BLACK;
                grandparent->color = RBTREE_RED;

                rbtree_left_rotate(grandparent);

                /* See the comment in the symmetric branch above. */
                return;
            }
        }

        uncle->color = RBTREE_BLACK;
        parent->color = RBTREE_BLACK;
        grandparent->color = RBTREE_RED;

        node = grandparent;
    }
}


void
rbtree_delete(struct rbtree *tree, rbtree_node *node)
{
    uint8_t color;
    rbtree_node *sentinel, *subst, *child;

    subst = node;
    sentinel = rbtree_sentinel(tree);

    if (node->left == sentinel) {
        child = node->right;

    } else if (node->right == sentinel) {
        child = node->left;

    } else {
        subst = rbtree_branch_min(tree, node->right);
        child = subst->right;
    }

    rbtree_parent_relink(child, subst);

    color = subst->color;

    if (subst != node) {
        /* Move the subst node to the deleted node position in the tree. */
        subst->color = node->color;

        subst->left = node->left;
        subst->left->parent = subst;

        subst->right = node->right;
        subst->right->parent = subst;

        rbtree_parent_relink(subst, node);
    }

    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;

    if (color == RBTREE_BLACK) {
        rbtree_delete_fixup(tree, child);
    }
}


static void
rbtree_delete_fixup(struct rbtree *tree, rbtree_node *node)
{
    rbtree_node *parent, *sibling;

    while (node != rbtree_root(tree) && node->color == RBTREE_BLACK) {
        /*
         * Prefetching parent nodes does not help here according
         * to microbenchmarks.
         */

        parent = node->parent;

        if (node == parent->left) {
            sibling = parent->right;

            if (sibling->color != RBTREE_BLACK) {
                sibling->color = RBTREE_BLACK;
                parent->color = RBTREE_RED;

                rbtree_left_rotate(parent);

                sibling = parent->right;
            }

            if (sibling->right->color == RBTREE_BLACK) {
                sibling->color = RBTREE_RED;

                if (sibling->left->color == RBTREE_BLACK) {
                    node = parent;
                    continue;
                }

                sibling->left->color = RBTREE_BLACK;

                rbtree_right_rotate(sibling);
                /*
                 * If the node is the leaf sentinel then the right
                 * rotate above changes its parent so a sibling below
                 * becames the leaf sentinel as well and this causes
                 * segmentation fault.  This is the reason why usual
                 * red-black tree implementations with a leaf sentinel
                 * which does not require to test leaf nodes at all
                 * nevertheless test the leaf sentinel in the left and
                 * right rotate procedures.  Since according to the
                 * algorithm node->parent must not be changed by both
                 * the left and right rotates above, it can be cached
                 * in a local variable.  This not only eliminates the
                 * sentinel test in rbtree_parent_relink() but also
                 * decreases the code size because C forces to reload
                 * non-restrict pointers.
                 */
                sibling = parent->right;
            }

            sibling->color = parent->color;
            parent->color = RBTREE_BLACK;
            sibling->right->color = RBTREE_BLACK;

            rbtree_left_rotate(parent);

            return;

        } else {
            sibling = parent->left;

            if (sibling->color != RBTREE_BLACK) {
                sibling->color = RBTREE_BLACK;
                parent->color = RBTREE_RED;

                rbtree_right_rotate(parent);

                sibling = parent->left;
            }

            if (sibling->left->color == RBTREE_BLACK) {
                sibling->color = RBTREE_RED;

                if (sibling->right->color == RBTREE_BLACK) {
                    node = parent;
                    continue;
                }

                sibling->right->color = RBTREE_BLACK;

                rbtree_left_rotate(sibling);

                /* See the comment in the symmetric branch above. */
                sibling = parent->left;
            }

            sibling->color = parent->color;
            parent->color = RBTREE_BLACK;
            sibling->left->color = RBTREE_BLACK;

            rbtree_right_rotate(parent);

            return;
        }
    }

    node->color = RBTREE_BLACK;
}


static inline void
rbtree_left_rotate(rbtree_node *node)
{
    rbtree_node *child;

    child = node->right;
    node->right = child->left;
    child->left->parent = node;
    child->left = node;

    rbtree_parent_relink(child, node);

    node->parent = child;
}


static inline void
rbtree_right_rotate(rbtree_node *node)
{
    rbtree_node *child;

    child = node->left;
    node->left = child->right;
    child->right->parent = node;
    child->right = node;

    rbtree_parent_relink(child, node);

    node->parent = child;
}


static inline void
rbtree_parent_relink(rbtree_node *subst, rbtree_node *node)
{
    rbtree_node *parent, **link;

    parent = node->parent;
    /*
     * The leaf sentinel's parent can be safely changed here.
     * See the comment in rbtree_delete_fixup() for details.
     */
    subst->parent = parent;
    /*
     * If the node's parent is the root sentinel it is safely changed
     * because the root sentinel's left child is the tree root.
     */
    link = (node == parent->left) ? &parent->left : &parent->right;
    *link = subst;
}


rbtree_node *
rbtree_destroy_next(struct rbtree *tree, rbtree_node **next)
{
    rbtree_node *node, *subst, *parent, *sentinel;

    sentinel = rbtree_sentinel(tree);

    /* Find the leftmost node. */
    for (node = *next; node->left != sentinel; node = node->left);

    /* Replace the leftmost node with its right child. */
    subst = node->right;
    parent = node->parent;

    parent->left = subst;
    subst->parent = parent;

    /*
     * The right child is used as the next start node.  If the right child
     * is the sentinel then parent of the leftmost node is used as the next
     * start node.  The parent of the root node is the sentinel so after
     * the single root node will be replaced with the sentinel, the next
     * start node will be equal to the sentinel and iteration will stop.
     */
    if (subst == sentinel) {
        subst = parent;
    }

    *next = subst;

    return node;
}
