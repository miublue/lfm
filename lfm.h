#ifndef __LFM_H
#define __LFM_H

#ifndef _USE_COLOR
#define _USE_COLOR 0
#endif

#define SHOW_HIDDEN 0

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

#define COLOR_STATUS COLOR_RED
#define COLOR_DIR    COLOR_BLUE
#define COLOR_EXEC   COLOR_GREEN
#define COLOR_LINK   COLOR_YELLOW

#if _USE_COLOR
#define ATTR_DIR    COLOR_PAIR(PAIR_DIR)|A_BOLD
#define ATTR_EXEC   COLOR_PAIR(PAIR_EXEC)
#define ATTR_LINK   COLOR_PAIR(PAIR_LINK)
#define ATTR_FILE   COLOR_PAIR(PAIR_NORMAL)
#define ATTR_STATUS COLOR_PAIR(PAIR_STATUS)|A_BOLD
#else
#define ATTR_DIR    A_BOLD
#define ATTR_EXEC   A_BOLD
#define ATTR_LINK   0
#define ATTR_FILE   0
#define ATTR_STATUS A_BOLD
#endif

enum { ACTION_NONE, ACTION_FIND, ACTION_EXEC, ACTION_OPEN, ACTION_MOVE, ACTION_COPY, ACTION_DELETE, };
enum { PAIR_NORMAL, PAIR_STATUS = 1, PAIR_DIR, PAIR_EXEC, PAIR_LINK };
enum { T_DIR, T_FILE, T_EXEC };

typedef struct {
    char name[ALLOC_SIZE];
    char is_link, type;
} file_t;

void init_lfm(char *path);
void quit_lfm(void);

void list_files(char *path);
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
void find_next(void);
void open_shell(void);
void edit_file(void);
void execute(char *cmd);

void update(void);
void render_files(void);
void render_status(void);

#endif
