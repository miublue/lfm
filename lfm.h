#ifndef __LFM_H
#define __LFM_H

#ifndef _USE_COLOR
#define _USE_COLOR 0
#endif

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

#include "config.h"

enum { ACTION_NONE, ACTION_FIND, ACTION_EXEC, ACTION_OPEN, ACTION_MOVE, ACTION_COPY, ACTION_DELETE, NUM_ACTIONS };
enum { PAIR_NORMAL, PAIR_STATUS = 1, PAIR_DIR, PAIR_EXEC, PAIR_LINK };
enum { T_DIR, T_FILE, T_EXEC };

typedef struct {
    char path[PATH_MAX], name[PATH_MAX];
    char is_link, type;
} file_t;

void init_lfm(char *path);
void quit_lfm(void);

void list_files(char *path);
void select_file(file_t file);
void select_all_files(bool add_all);

void scroll_up(void);
void scroll_down(void);
void move_left(void);
void move_right(void);
void move_up(void);
void move_down(void);
void move_home(void);
void move_end(void);
void page_up(void);
void page_down(void);
void toggle_hidden(void);
void open_shell(void);
void edit_file(void);
void reload_files(void);
void execute(char *cmd);
void find_next(char *str, int sz);

void update(void);
void render_files(void);
void render_status(void);

#endif
