/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Red Black Trees
 * (C) 1999  Andrea Arcangeli <andrea@suse.de>
 * Taken from the Linux 2.6.30 source.
 */

/*
 * To use rbtrees you'll have to implement your own insert and search cores.
 * This will avoid us to use callbacks and to drop drammatically performances.
 * I know it's not the cleaner way,  but in C (not in C++) to get
 * performances and genericity...
 *
 * Some example of insert and search follows here. The search is a plain
 * normal search over an ordered tree. The insert instead must be implemented
 * int two steps: as first thing the code must insert the element in
 * order as a red leaf in the tree, then the support library function
 * rb_insert_color() must be called. Such function will do the
 * not trivial work to rebalance the rbtree if necessary.
 *
 * -----------------------------------------------------------------------
 *
 * static inline struct page * rb_search_page_cache(struct inode * inode,
 *						 unsigned long offset)
 * {
 *	struct rb_node * n = inode->i_rb_page_cache.rb_node;
 *	struct page * page;
 *
 *	while (n)
 *	{
 *		page = rb_entry(n, struct page, rb_page_cache);
 *
 *		if (offset < page->offset)
 *			n = n->rb_left;
 *		else if (offset > page->offset)
 *			n = n->rb_right;
 *		else
 *			return page;
 *	}
 *	return NULL;
 * }
 *
 * static inline struct page * __rb_insert_page_cache(struct inode * inode,
 *						   unsigned long offset,
 *						   struct rb_node * node)
 * {
 *	struct rb_node ** p = &inode->i_rb_page_cache.rb_node;
 *	struct rb_node * parent = NULL;
 *	struct page * page;
 *
 *	while (*p)
 *	{
 *		parent = *p;
 *		page = rb_entry(parent, struct page, rb_page_cache);
 *
 *		if (offset < page->offset)
 *			p = &(*p)->rb_left;
 *		else if (offset > page->offset)
 *			p = &(*p)->rb_right;
 *		else
 *			return page;
 *	}
 *
 *	rb_link_node(node, parent, p);
 *
 *	return NULL;
 * }
 *
 * static inline struct page * rb_insert_page_cache(struct inode * inode,
 *						 unsigned long offset,
 *						 struct rb_node * node)
 * {
 *	struct page * ret;
 *	if ((ret = __rb_insert_page_cache(inode, offset, node)))
 *		goto out;
 *	rb_insert_color(node, &inode->i_rb_page_cache);
 *  out:
 *	return ret;
 * }
 */

#ifndef	_LINUX_RBTREE_H
#define	_LINUX_RBTREE_H

#include <stddef.h>

struct rb_node {
	unsigned long  rb_parent_color;
#define	RB_RED		0
#define	RB_BLACK	1
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root {
	struct rb_node *rb_node;
};

#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)   ((r)->rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}

static inline void rb_set_color(struct rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_ROOT	(struct rb_root) { NULL, }
#define	rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)	(!(root)->rb_node)
#define RB_EMPTY_NODE(node)	(rb_parent(node) == node)
#define RB_CLEAR_NODE(node)	(rb_set_parent(node, node))

void rb_insert_color(struct rb_node *node, struct rb_root *rb_root);
void rb_erase(struct rb_node *node, struct rb_root *rb_root);

/* Find logical next and previous nodes in a tree */
struct rb_node *rb_next(const struct rb_node *node);
struct rb_node *rb_prev(const struct rb_node *node);
struct rb_node *rb_first(const struct rb_root *rb_root);
struct rb_node *rb_last(const struct rb_root *rb_root);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
void rb_replace_node(struct rb_node *victim, struct rb_node *new,
		     struct rb_root *root);

static inline void rb_link_node(struct rb_node *node, struct rb_node *parent,
				struct rb_node **rb_link)
{
	node->rb_parent_color = (unsigned long)parent;
	node->rb_left = NULL;
	node->rb_right = NULL;

	*rb_link = node;
}

#endif	/* _LINUX_RBTREE_H */
