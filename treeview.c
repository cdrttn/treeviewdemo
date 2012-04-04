#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ncurses.h>
#include <menu.h>

struct tree_node {

	char *name;
	char *label;
	
	struct tree_node *parent;
	struct tree_node *child_head;
	struct tree_node *next;
};

struct tree_view {
	
	struct tree_node *root;
	WINDOW *window;
	WINDOW *sub_window;
	MENU *menu;
	ITEM **current_items;
	const char *back_label;
};

struct tree_node *tree_node_new(struct tree_node *parent, const char *name)
{
	struct tree_node *node;

	node = calloc(1, sizeof(struct tree_node));
	if (node == NULL)
		return NULL;

	node->name = strdup(name);

	if (parent) {
		if (!parent->child_head) {
			parent->child_head = node;
		}
		node->parent = parent;
	}

	return node;
}

void tree_node_add_child(struct tree_node *node, struct tree_node *child)
{
	assert(node->child_head == NULL);
	assert(child->parent == NULL);
	node->child_head = child;
	child->parent = node;
}

void tree_node_free(struct tree_node *node)
{
	assert(node->child_head == NULL);
	assert(node->next == NULL);
	free(node->name);
	free(node);
}

void tree_node_free_recursive(struct tree_node *node)
{
	struct tree_node *tmp, *next;

	if (node == NULL) {
		return;
	}

	for (tmp = node, next = tmp->next; tmp != NULL; tmp = next) {
		if (node->child_head) {
			tree_node_free_recursive(node->child_head);
		}
		node->child_head = NULL;
		node->next = NULL;
		tree_node_free(node);
	}
}

void tree_view_update(struct tree_view *view, struct tree_node *list)
{
	ITEM **items, *item;
	struct tree_node *tmp;
	size_t i, n_items;

	if (list == NULL) {
		list = view->root;
	}

	for (n_items = 0, tmp = list; tmp != NULL; tmp = tmp->next) {
		n_items++;
	}

	/* add a back label? */	
	if (list->parent) {
		items = calloc(n_items + 2, sizeof(ITEM **));
		i = 1;
		items[0] = new_item(view->back_label, view->back_label);
	} else {
		items = calloc(n_items + 1, sizeof(ITEM **));
		i = 0;
	}

	for (tmp = list; tmp != NULL; ++i, tmp = tmp->next) {
		const char *label = tmp->name;

		if (tmp->child_head) {
			char buf[32];
			snprintf(buf, sizeof(buf), "+%s", tmp->name);
			assert(tmp->label == NULL);
			tmp->label = strdup(buf);
			label = tmp->label;
		}

		items[i] = new_item(label, tmp->name);
		set_item_userptr(items[i], tmp);
	}

	if (view->menu == NULL) {
		view->menu = new_menu(items);
		set_menu_win(view->menu, view->window);
		set_menu_sub(view->menu, view->sub_window);
		menu_opts_off(view->menu, O_SHOWDESC);
		set_menu_mark(view->menu, "* ");
	} else {
		unpost_menu(view->menu);
		set_menu_items(view->menu, items);
	}

	if (view->current_items) {
		for (i = 0; view->current_items[i] != NULL; ++i) {
			item = view->current_items[i];
			tmp = item_userptr(item);
			if (tmp && tmp->label) {
				free(tmp->label);
				tmp->label = NULL;
			}
			free_item(item);
		}
		free(view->current_items);
		view->current_items = NULL;
	}
	view->current_items = items;
}

void tree_view_show(struct tree_view *view)
{
	post_menu(view->menu);
	wrefresh(view->window);
}

struct tree_view *tree_view_new(struct tree_node *root, WINDOW *orig,
	int nlines, int ncols, int begin_y, int begin_x)
{
	struct tree_view *view;
	static const char *back = "..";

	view = calloc(1, sizeof(struct tree_view));

	view->window = orig;
	view->sub_window = derwin(orig, nlines, ncols, begin_y, begin_x);
	view->root = root;
	view->back_label = back;
	tree_view_update(view, root);

	return view;
}

void tree_view_free(struct tree_view *view)
{
	free_menu(view->menu);
	tree_node_free_recursive(view->root);
	free(view->current_items);
}

#define LIST_LEN 8
#define LIST_DEPTH 3

void seed_tree_recursive(struct tree_node *parent, int depth)
{
	struct tree_node *node, *prev;
	int i;

	if (depth > LIST_DEPTH) {
		return;
	}

	for (prev = NULL, i = 0; i < LIST_LEN; ++i) {
		char buf[32];
		snprintf(buf, sizeof(buf), "item[%d,%d]", depth, i);
		node = tree_node_new(parent, buf);
		//printf("%s\n", buf);
		seed_tree_recursive(node, depth + 1);
		if (prev)
			prev->next = node;
		prev = node;
	}	
}

struct tree_node *seed_tree()
{
	struct tree_node *root;

	root = tree_node_new(NULL, "ROOT");
	seed_tree_recursive(root, 0);
	
	return root;	
}

int main()
{
	WINDOW *tree_window;
	struct tree_view *view;
	struct tree_node *root;
	struct tree_node *node;
	int c;

	root = seed_tree();

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	tree_window = newwin(10, 40, 4, 4);
	keypad(tree_window, TRUE);
	view = tree_view_new(root, tree_window, 10, 40, 0, 0);	
	refresh();
	tree_view_show(view);

	while ((c = wgetch(tree_window)) != 'q') {
		switch (c) {
		case KEY_DOWN:
			menu_driver(view->menu, REQ_DOWN_ITEM);
			break;
		case KEY_UP:
			menu_driver(view->menu, REQ_UP_ITEM);
			break;
		case KEY_RIGHT:
			node = item_userptr(current_item(view->menu));
			if (node && node->child_head)
				tree_view_update(view, node->child_head);
			break;
		case KEY_LEFT:
			node = item_userptr(current_item(view->menu));
			if (node && node->parent)
				tree_view_update(view, node->parent);
			break;
		}
		tree_view_show(view);
		
	}

	endwin();

	return 0;
}
