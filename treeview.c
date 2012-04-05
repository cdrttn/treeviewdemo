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
	struct tree_node *previous;
	struct tree_node *next;
};

struct tree_view {
	
	struct tree_node *root;
	WINDOW *window;
	WINDOW *sub_window;
	MENU *menu;
	ITEM **current_items;
};

struct tree_node *tree_node_new(struct tree_node *parent, const char *name)
{
	struct tree_node *node;

	node = calloc(1, sizeof(struct tree_node));
	if (node == NULL)
		return NULL;

	node->name = strdup(name);

	if (parent) {
		/* Check if this node is the first descendant of parent. */
		if (!parent->child_head) {
			parent->child_head = node;
		}
		node->parent = parent;
	}

	return node;
}

void tree_node_append(struct tree_node *left, struct tree_node *right)
{
	if (left->next) {
		right->next = left->next;
		left->next->previous = right;
	}	
	left->next = right;
	right->previous = left;
}

struct tree_node *tree_node_pop(struct tree_node **plist)
{
	struct tree_node *node;

	node = *plist;

	if (node == NULL)
		return NULL;

	*plist = node->previous;
	if (*plist == NULL)
		*plist = node->next;
	if (node->previous) {
		node->previous->next = node->next;
	}
	if (node->next) {
		node->next->previous = node->previous;
	}

	node->next = NULL;
	node->previous = NULL;

	return node;
}

struct tree_node *tree_node_first(struct tree_node *list)
{
	/* Grab the first node in this list from the parent if available. */
	if (list->parent) {
		return list->parent->child_head;
	}

	while (list && list->previous) {
		list = list->previous;
	}

	return list;
}

void tree_node_free(struct tree_node *node)
{
	assert(node->child_head == NULL);
	assert(node->next == NULL);
	assert(node->previous == NULL);
	free(node->name);
	free(node);
}

void tree_node_free_recursive(struct tree_node *list)
{
	struct tree_node *node;

	if (list == NULL) {
		return;
	}

	while ((node = tree_node_pop(&list)) != NULL) {
		if (node->child_head) {
			tree_node_free_recursive(node->child_head);
		}
		node->child_head = NULL;
		tree_node_free(node);
	}
}

void tree_view_free_current_items(struct tree_view *view)
{
	size_t i;
	struct tree_node *tmp;
	ITEM *item;

	if (view->current_items == NULL) {
		return;
	}

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

void tree_view_update(struct tree_view *view, struct tree_node *list)
{
	ITEM **items;
	struct tree_node *tmp;
	size_t i, n_items;

	if (list == NULL) {
		list = view->root;
	}
	for (n_items = 0, tmp = list; tmp != NULL; tmp = tmp->next) {
		n_items++;
	}
	items = calloc(n_items + 1, sizeof(ITEM **));

	for (i = 0, tmp = list; tmp != NULL; ++i, tmp = tmp->next) {
		const char *label = tmp->name;

		/* Add a '+' marker to indicate that the item has
		   descendants. */
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


	unpost_menu(view->menu);
	set_menu_items(view->menu, items);
	tree_view_free_current_items(view);
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
	static const char *dummy = "12345";
	
	view = calloc(1, sizeof(struct tree_view));
	view->window = orig;
	view->sub_window = derwin(orig, nlines, ncols, begin_y, begin_x);
	view->root = root;

	view->current_items = calloc(2, sizeof(ITEM **));
	view->current_items[0] = new_item(dummy, dummy);

	view->menu = new_menu(view->current_items);
	set_menu_format(view->menu, nlines, 1);
	set_menu_win(view->menu, view->window);
	set_menu_sub(view->menu, view->sub_window);
	menu_opts_off(view->menu, O_SHOWDESC);
	set_menu_mark(view->menu, "* ");

	tree_view_update(view, root);

	return view;
}

void tree_view_free(struct tree_view *view)
{
	unpost_menu(view->menu);
	free_menu(view->menu);
	tree_view_free_current_items(view);
	tree_node_free_recursive(view->root);
}

#define LIST_LEN 25
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
		if (prev) {
			tree_node_append(prev, node);
		}
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

void print_path_recursive(WINDOW *label, struct tree_node *node)
{
	if (node->parent)
		print_path_recursive(label, node->parent);

	wprintw(label, "%s/", node->name);
}

void print_path(WINDOW *label, struct tree_node *node)
{
	if (node == NULL)
		return;

	wmove(label, 0, 0);
	wclrtoeol(label);
	wprintw(label, "/");
	wmove(label, 0, 0);

	if (node->parent)
		print_path_recursive(label, node->parent);

	wnoutrefresh(label);
	wrefresh(label);
}

int main()
{
	WINDOW *tree_window, *path_label;
	struct tree_view *view;
	struct tree_node *root;
	struct tree_node *node;
	int c;

	root = seed_tree();

	initscr();
	start_color();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	tree_window = newwin(25, 80, 4, 4);
	mvwprintw(tree_window, 0, 0, "Path: ");
	path_label = derwin(tree_window, 1, 45, 0, 6);
	wprintw(path_label, "/");

	mvwprintw(tree_window, 19, 0,
			"Navigation: UP and DOWN to move around menu.");
	mvwprintw(tree_window, 20, 12,
			"LEFT and RIGHT to ascend and descend.");

	keypad(tree_window, TRUE);
	view = tree_view_new(root, tree_window, 15, 40, 3, 0);	
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
			if (node && node->child_head) {
				print_path(path_label, node->child_head);
				tree_view_update(view, node->child_head);
			}
			break;
		case KEY_LEFT:
			node = item_userptr(current_item(view->menu));
			if (node && node->parent) {
				print_path(path_label, node->parent);
				tree_view_update(view,
					tree_node_first(node->parent));
			}
			break;
		}
		tree_view_show(view);
		
	}

	endwin();
	tree_view_free(view);

	return 0;
}
