#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ncurses.h>
#include "inputbox.h"

#define SHOW_HIDDEN 0

#define CTRL(c) ((c) & 0x1f)
#define ALLOC_SIZE 512

#define ATTR_DIR    COLOR_PAIR(PAIR_DIR)|A_BOLD
#define ATTR_LINK   COLOR_PAIR(PAIR_LINK)
#define ATTR_EXEC   COLOR_PAIR(PAIR_EXEC)
#define ATTR_FILE   COLOR_PAIR(PAIR_NORMAL)
#define ATTR_STATUS COLOR_PAIR(PAIR_STATUS)|A_BOLD

enum { ACTION_NONE, ACTION_FIND, ACTION_EXEC, ACTION_OPEN, ACTION_MOVE, ACTION_COPY, };
enum { PAIR_NORMAL, PAIR_STATUS = 1, PAIR_DIR, PAIR_EXEC, PAIR_LINK };
enum { T_DIR, T_FILE, T_EXEC };

typedef struct {
    char name[ALLOC_SIZE];
    char is_link, type;
} file_t;

struct {
    char *path;
    size_t files_sz, files_alloc;
    file_t *files;
    int cur, off, ww, wh, hidden, action;
    inputbox_t input;
} lfm;

void init_lfm(char *path);
void quit_lfm();
void init_curses();
void quit_curses();

void list_files(char *path);
void scroll_up();
void scroll_down();
void move_left();
void move_right();
void move_up();
void move_down();
void move_home();
void move_end();
void page_up();
void page_down();
void toggle_hidden();
void find_next();
void open_shell();
void edit_file();
void execute(char *cmd);

void update();
void render_files();
void render_status();

int ch = 0;

void init_lfm(char *path) {
    lfm.path = NULL;
    lfm.hidden = SHOW_HIDDEN;
    lfm.files = malloc(sizeof(file_t) * (lfm.files_alloc = ALLOC_SIZE));
    lfm.action = ACTION_NONE;
    list_files(path);
    input_reset(&lfm.input);
}

void quit_lfm() {
    char path[ALLOC_SIZE] = {0};
    sprintf(path, "%s/.lfmsel", getenv("HOME"));
    FILE *file = fopen(path, "w");
    fwrite(lfm.path, strlen(lfm.path), 1, file);
    fclose(file);
    free(lfm.files);
    free(lfm.path);
    quit_curses();
    exit(0);
}

static file_t _stat_file(char *name) {
    char path[ALLOC_SIZE] = {0};
    sprintf(path, "%s/%s", lfm.path, name);
    struct stat file_stat;
    stat(path, &file_stat);
    file_t file = {
        .is_link = S_ISLNK(file_stat.st_mode),
        .name = {0},
    };
    file.type = S_ISDIR(file_stat.st_mode)? T_DIR : (file_stat.st_mode & S_IXUSR)? T_EXEC : T_FILE;
    memcpy(file.name, name, strlen(name));
    return file;
}

static void _append_file(file_t f) {
    if (lfm.files_sz >= lfm.files_alloc)
        lfm.files = realloc(lfm.files, sizeof(file_t) * (lfm.files_alloc += ALLOC_SIZE));
    lfm.files[lfm.files_sz++] = f;
}

static int _compare_files(const void *a_ptr, const void *b_ptr) {
    const file_t *a = (file_t*)a_ptr, *b = (file_t*)b_ptr;
    if (a->type == T_DIR && b->type != T_DIR) return -1;
    if (a->type != T_DIR && b->type == T_DIR) return 1;
    else return (strcmp(a->name, b->name));
}

void list_files(char *path) {
    if (lfm.path) free(lfm.path);
    lfm.path = realpath(path, NULL);
    if (!lfm.path) goto fail;
    lfm.files_sz = lfm.cur = lfm.off = 0;

    struct dirent *ent = NULL;
    DIR *dir = opendir(lfm.path);
    if (!dir) goto fail;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (!lfm.hidden && ent->d_name[0] == '.') continue;
        file_t file = _stat_file(ent->d_name);
        _append_file(file);
    }

    qsort(lfm.files, lfm.files_sz, sizeof(file_t), &_compare_files);
    closedir(dir);
    return;
fail:
    fprintf(stderr, "error: path '%s' does not exist\n", path);
    exit(1);
}

static void update_list_files() {
    char path[ALLOC_SIZE] = {0};
    int cur = lfm.cur, off = lfm.off;
    sprintf(path, "%s", lfm.path);
    list_files(path);
    if (!lfm.files_sz) {
        lfm.cur = lfm.off = 0;
        return;
    }
    lfm.cur = cur; lfm.off = off;
    while (lfm.cur >= 0 && lfm.cur >= lfm.files_sz) move_up();
}

void init_curses() {
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    use_default_colors();
    start_color();
    init_pair(PAIR_NORMAL, -1, -1);
    init_pair(PAIR_STATUS, COLOR_RED,   -1);
    init_pair(PAIR_DIR,    COLOR_BLUE,  -1);
    init_pair(PAIR_EXEC,   COLOR_GREEN, -1);
    init_pair(PAIR_LINK,   COLOR_CYAN,  -1);
}

void quit_curses() {
    endwin();
    curs_set(1);
}

void scroll_up() {
    if (lfm.cur-lfm.off < 0 && lfm.off > 0) --lfm.off;
}

void scroll_down() {
    if (lfm.cur-lfm.off > lfm.wh-2) ++lfm.off;
}

void move_left() {
    char prev[ALLOC_SIZE] = {0}, path[ALLOC_SIZE] = {0}, found = 0;
    // XXX: fix listing root directory (could be a cygwin bug lmao)

    int sz = strlen(lfm.path)-1;
    for (; sz >= 0 && lfm.path[sz] != '/'; --sz);
    memcpy(prev, lfm.path+sz+1, strlen(lfm.path)-sz);
    if (sz <= 1 || strcmp(lfm.path, "/") == 0) memcpy(path, "/", 1);
    else memcpy(path, lfm.path, sz);

    list_files(path);
    for (int i = 0; i < lfm.files_sz; ++i) {
        if (!strcmp(lfm.files[i].name, prev)) {
            found = 1;
            break;
        }
        move_down();
    }
    if (!found) lfm.off = lfm.cur = 0;
}

void move_right() {
    file_t file = lfm.files[lfm.cur];
    if (file.type != T_DIR) return;
    char path[ALLOC_SIZE] = {0};
    sprintf(path, "%s/%s", lfm.path, file.name);
    list_files(path);
}

void move_up() {
    if (lfm.cur == 0) return;
    --lfm.cur;
    scroll_up();
}

void move_down() {
    if (lfm.cur >= lfm.files_sz-1) return;
    ++lfm.cur;
    scroll_down();
}

void move_home() {
    lfm.cur = lfm.off = 0;
}

void move_end() {
    move_home();
    while (lfm.cur < lfm.files_sz-1) move_down();
}

void page_up() {
    for (int i = 0; i < lfm.wh-2; ++i) move_up();
}

void page_down() {
    for (int i = 0; i < lfm.wh-2; ++i) move_down();
}

void toggle_hidden() {
    char *file = strdup(lfm.files[lfm.cur].name);
    lfm.hidden = !lfm.hidden;
    update_list_files();
    if (lfm.files_sz && strcmp(lfm.files[lfm.cur].name, file) != 0) {
        memcpy(lfm.input.text, file, (lfm.input.text_sz = strlen(file)));
        find_next();
    }
    free(file);
}

void find_next() {
    int found = 0, where = 0;
    char to_find[ALLOC_SIZE] = {0};
    memcpy(to_find, lfm.input.text, lfm.input.text_sz);
    for (where = lfm.cur+1; where < lfm.files_sz; ++where) {
        if (strstr(lfm.files[where].name, to_find)) {
            found = 1; break;
        }
    }
    if (!found) {
        for (where = 0; where < lfm.cur; ++where) {
            if (strstr(lfm.files[where].name, to_find)) {
                found = 1; break;
            }
        }
    }
    if (found) for (lfm.cur = lfm.off = 0; lfm.cur < where; move_down());
}

void open_shell() {
    execute("$SHELL");
}

void edit_file() {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "$EDITOR '%s'", lfm.files[lfm.cur].name);
    execute(cmd);
}

void execute(char *cmd) {
    char s[ALLOC_SIZE] = {0};
    sprintf(s, "cd '%s'; %s", lfm.path, cmd);
    quit_curses();
    system(s);
    update_list_files();
    init_curses();
}

static void render_file(int l) {
    file_t file = lfm.files[l];
    char postfix[3] = {0};
    int attr = 0;

    if (lfm.cur == l) attr |= A_REVERSE;
    switch (file.type) {
    case T_DIR:  attr |= ATTR_DIR;  strcat(postfix, "/"); break;
    case T_EXEC: attr |= ATTR_EXEC; strcat(postfix, "*"); break;
    default:     attr |= ATTR_FILE; break;
    }
    if (file.is_link) { attr |= ATTR_LINK; strcat(postfix, "@"); }

    attron(attr);
    mvprintw(l-lfm.off, 1, "%s%s", file.name, postfix);
    attroff(attr);
}

void render_files() {
    erase();
    if (lfm.files_sz == 0) {
        attron(A_REVERSE);
        mvprintw(0, 0, "  empty  ");
        attroff(A_REVERSE);
    }
    for (int i = lfm.off; i < lfm.off+lfm.wh-1; ++i) {
        if (i >= lfm.files_sz) break;
        render_file(i);
    }
}

static inline char *_action_to_cstr() {
    switch (lfm.action) {
    case ACTION_FIND: return "Find: ";
    case ACTION_EXEC: return "Exec: ";
    case ACTION_OPEN: return "Open: ";
    case ACTION_MOVE: return "Move: ";
    case ACTION_COPY: return "Copy: ";
    default: return "None";
    }
}

void render_status() {
    char status[ALLOC_SIZE] = {0};
    sprintf(status, " %d:%ld %s ",
        // ch, keyname(ch),
        lfm.cur+1, lfm.files_sz, lfm.path);
    const size_t status_sz = strlen(status);
    attron(ATTR_STATUS);
    mvprintw(lfm.wh-1, lfm.ww-status_sz, status);

    if (lfm.action != ACTION_NONE) {
        char *astr = _action_to_cstr();
        mvprintw(lfm.wh-1, 0, "%s", astr);
        input_render(&lfm.input, strlen(astr), lfm.wh-1, lfm.ww-status_sz-strlen(astr));
    }
    attroff(ATTR_STATUS);
}

static inline int update_none(int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        lfm.action = ACTION_NONE;
        return 1;
    }
    return 0;
}

static void update_find(int ch) {
    if (update_none(ch)) return;
    if (ch == '\n' || ch == CTRL('f') || ch == CTRL('n')) {
        if (lfm.input.text_sz) find_next();
        return;
    }
    input_update(&lfm.input, ch);
}

static void update_exec(int ch) {
    if (update_none(ch)) return;
    if (ch == '\n') {
        char cmd[ALLOC_SIZE] = {0};
        sprintf(cmd, "cd '%s'; %.*s", lfm.path, lfm.input.text_sz, lfm.input.text);
        execute(cmd);
        update_list_files();
        lfm.action = ACTION_NONE;
        return;
    }
    input_update(&lfm.input, ch);
}

static void update_open(int ch) {
    if (update_none(ch)) return;
    if (ch == '\n') {
        char cmd[ALLOC_SIZE] = {0};
        sprintf(cmd, "cd '%s'; %.*s '%s'", lfm.path, lfm.input.text_sz, lfm.input.text, lfm.files[lfm.cur].name);
        execute(cmd);
        update_list_files();
        lfm.action = ACTION_NONE;
        return;
    }
    input_update(&lfm.input, ch);
}

static void update_move(int ch) {
    if (update_none(ch)) return;
    if (ch == '\n') {
        char cmd[ALLOC_SIZE] = {0};
        char *move_or_copy = lfm.action == ACTION_MOVE? "mv" : "cp";
        sprintf(cmd, "cd '%s'; %s '%s' '%.*s'", lfm.path,
            move_or_copy, lfm.files[lfm.cur].name,
            lfm.input.text_sz, lfm.input.text);
        execute(cmd);
        update_list_files();
        lfm.action = ACTION_NONE;
        return;
    }
    input_update(&lfm.input, ch);
}

void update() {
    ch = getch();
    switch (lfm.action) {
    case ACTION_FIND:
        return update_find(ch);
    case ACTION_EXEC:
        return update_exec(ch);
    case ACTION_OPEN:
        return update_open(ch);
    case ACTION_MOVE:
        return update_move(ch);
    case ACTION_COPY:
        return update_move(ch);
    default: break;
    }

    switch (ch) {
   case 'q': case 'Q': case CTRL('q'):
        return quit_lfm();
    case CTRL('f'):
        lfm.action = ACTION_FIND;
        return input_reset(&lfm.input);
    case CTRL('n'):
        if (lfm.input.text_sz) find_next();
        return;
    case ':':
        lfm.action = ACTION_EXEC;
        return input_reset(&lfm.input);
    case 'o': case 'O': case CTRL('o'):
        if (!lfm.files_sz) break;
        lfm.action = ACTION_OPEN;
        return input_reset(&lfm.input);
    case 'v': case 'V': case 'm': case 'M': case 'r': case 'R':
        if (!lfm.files_sz) break;
        lfm.action = ACTION_MOVE;
        return input_set(&lfm.input, lfm.files[lfm.cur].name, strlen(lfm.files[lfm.cur].name));
    case 'c': case 'C':
        if (!lfm.files_sz) break;
        lfm.action = ACTION_COPY;
        return input_set(&lfm.input, lfm.files[lfm.cur].name, strlen(lfm.files[lfm.cur].name));
    case KEY_LEFT:
        return move_left();
    case KEY_RIGHT:
        return move_right();
    case KEY_UP:
        return move_up();
    case KEY_DOWN:
        return move_down();
    case KEY_HOME:
        return move_home();
    case KEY_END:
        return move_end();
    case KEY_PPAGE:
        return page_up();
    case KEY_NPAGE:
        return page_down();
    case '.':
        return toggle_hidden();
    case 's': case 'S': case '!':
        return open_shell();
    case 'e': case 'E':
        if (!lfm.files_sz) break;
        return edit_file();
    default: break;
    }
}

void main(int argc, char **argv) {
    init_lfm(argc < 2? "." : argv[1]);
    init_curses();
    for (;;) {
        getmaxyx(stdscr, lfm.wh, lfm.ww);
        render_files();
        render_status();
        update();
    }
    quit_lfm();
}
