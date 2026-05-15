#ifndef __LFM_H
#define __LFM_H

#ifndef _USE_COLOR
#define _USE_COLOR 0
#endif

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))

#define MIN_TERM_WIDTH 30
#define MIN_TERM_HEIGHT 5

#include "config.h"

enum { MODE_NONE, MODE_FIND, MODE_EXEC, MODE_OPEN, MODE_MOVE, MODE_COPY, MODE_DELETE, MODE_PICKER, NUM_ACTIONS };
enum { PAIR_NORMAL, PAIR_STATUS = 1, PAIR_DIR, PAIR_EXEC, PAIR_LINK };
enum { T_DIR, T_FILE, T_EXEC };

struct file {
    char path[PATH_MAX], name[PATH_MAX];
    char is_link, type;
};

struct files_list {
    size_t sz, cap;
    struct file *buf;
};

struct tab {
    char *path;
    struct files_list files;
    int cur, off, show_hidden;
};

void init_lfm(char *path);
void quit_lfm(char *path);

void list_files(struct tab *tab, char *path);
void select_file(struct file file);
void select_all_files(struct tab *tab, bool add_all);

void scroll_up(struct tab *tab);
void scroll_down(struct tab *tab);
void move_left(struct tab *tab);
void move_right(struct tab *tab);
void move_up(struct tab *tab);
void move_down(struct tab *tab);
void move_home(struct tab *tab);
void move_end(struct tab *tab);
void page_up(struct tab *tab);
void page_down(struct tab *tab);
void toggle_hidden(struct tab *tab);
void open_shell(struct tab *tab);
void edit_file(struct tab *tab);
void reload_files(struct tab *tab);
void execute(struct tab *tab, char *cmd);
void find_next(struct tab *tab, char *str, int sz);
struct tab *create_tab(char *path);
void close_tab(struct tab *tab);
char *expand_home(const char *path);

void update(struct tab *tab);
void render_files(struct tab *tab);
void render_status(void);

#endif
