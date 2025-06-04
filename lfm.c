#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ncurses.h>
#include "inputbox.h"
#include "lfm.h"

#define SELECTION_TEXT "selection"

typedef struct {
    size_t sz, cap;
    file_t *buf;
} list_files_t;

static struct {
    char *path;
    list_files_t files;
    list_files_t selection;
    int cur, off, ww, wh, hidden, action;
    inputbox_t input;
} lfm;

static void _init_curses(void) {
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
#if _USE_COLOR
    use_default_colors();
    start_color();
    init_pair(PAIR_NORMAL,           -1, -1);
    init_pair(PAIR_STATUS, COLOR_STATUS, -1);
    init_pair(PAIR_DIR,    COLOR_DIR,    -1);
    init_pair(PAIR_EXEC,   COLOR_EXEC,   -1);
    init_pair(PAIR_LINK,   COLOR_LINK,   -1);
#endif
}

static void _quit_curses(void) {
    endwin();
    curs_set(1);
}

static void _init_files(list_files_t *list) {
    list->buf = malloc(sizeof(file_t) * (list->cap = ALLOC_SIZE));
    list->sz = 0;
}

static void _free_files(list_files_t *list) {
    free(list->buf);
    list->sz = list->cap = 0;
}

static void _append_file(list_files_t *list, file_t file) {
    if (list->sz >= list->cap)
        list->buf = realloc(list->buf, sizeof(file_t) * (list->cap += ALLOC_SIZE));
    list->buf[list->sz++] = file;
}

// return index of file if selected, else return -1
static int _find_file(list_files_t *list, file_t file) {
    for (int i = 0; i < list->sz; ++i) {
        file_t *f = &list->buf[i];
        if (!strcmp(f->path, file.path) && !strcmp(f->name, file.name)) return i;
    }
    return -1;
}

static void _remove_file(list_files_t *list, int idx) {
    if (list->sz == 0 || idx >= list->sz) return;
    --list->sz;
    for (int i = idx; i < list->sz; ++i) list->buf[i] = list->buf[i+1];
}

void init_lfm(char *path) {
    lfm.path = NULL;
    lfm.hidden = SHOW_HIDDEN;
    lfm.action = ACTION_NONE;
    _init_files(&lfm.files);
    _init_files(&lfm.selection);
    list_files(path);
    input_reset(&lfm.input);
}

void quit_lfm(void) {
    char path[PATH_MAX] = {0};
    sprintf(path, "%s/.lfmsel", getenv("HOME"));
    FILE *file = fopen(path, "w");
    fwrite(lfm.path, strlen(lfm.path), 1, file);
    fclose(file);
    _free_files(&lfm.files);
    _free_files(&lfm.selection);
    free(lfm.path);
    _quit_curses();
    exit(0);
}

static file_t _stat_file(char *name) {
    char path[PATH_MAX] = {0};
    sprintf(path, "%s/%s", lfm.path, name);
    struct stat file_stat;
    stat(path, &file_stat);
    file_t file = {
        .is_link = S_ISLNK(file_stat.st_mode),
        .name = {0},
        .path = {0},
    };
    file.type = S_ISDIR(file_stat.st_mode)? T_DIR : (file_stat.st_mode & S_IXUSR)? T_EXEC : T_FILE;
    memcpy(file.name, name, strlen(name));
    memcpy(file.path, lfm.path, strlen(lfm.path));
    return file;
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
    chdir(lfm.path);
    lfm.files.sz = lfm.cur = lfm.off = 0;

    struct dirent *ent = NULL;
    DIR *dir = opendir(lfm.path);
    if (!dir) goto fail;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (!lfm.hidden && ent->d_name[0] == '.') continue;
        file_t file = _stat_file(ent->d_name);
        _append_file(&lfm.files, file);
    }

    qsort(lfm.files.buf, lfm.files.sz, sizeof(file_t), &_compare_files);
    closedir(dir);
    return;
fail:
    fprintf(stderr, "error: path '%s' does not exist\n", path);
    exit(1);
}

void select_file(file_t file) {
    int idx = _find_file(&lfm.selection, file);
    if (idx != -1) _remove_file(&lfm.selection, idx);
    else _append_file(&lfm.selection, file);
}

void select_all_files(bool add_all) {
    for (int i = 0; i < lfm.files.sz; ++i) {
        if (_find_file(&lfm.selection, lfm.files.buf[i]) == -1 || !add_all) {
            select_file(lfm.files.buf[i]);
        }
    }
}

void reload_files(void) {
    char path[PATH_MAX] = {0};
    int cur = lfm.cur, off = lfm.off;
    sprintf(path, "%s", lfm.path);
    list_files(path);
    if (!lfm.files.sz) {
        lfm.cur = lfm.off = 0;
        return;
    }
    lfm.cur = cur; lfm.off = off;
    while (lfm.cur >= 0 && lfm.cur >= lfm.files.sz) move_up();
}

void scroll_up(void) {
    if (lfm.cur-lfm.off < 0 && lfm.off > 0) --lfm.off;
}

void scroll_down(void) {
    if (lfm.cur-lfm.off > lfm.wh-2) ++lfm.off;
}

void move_left(void) {
    char prev[PATH_MAX] = {0}, path[PATH_MAX] = {0}, found = 0;
    int sz = strlen(lfm.path)-1;
    for (; sz >= 0 && lfm.path[sz] != '/'; --sz);
    memcpy(prev, lfm.path+sz+1, strlen(lfm.path)-sz);
    sprintf(path, "%s/..", lfm.path);
    list_files(path);
    for (int i = 0; i < lfm.files.sz; ++i) {
        if (!strcmp(lfm.files.buf[i].name, prev)) {
            found = 1;
            break;
        }
        move_down();
    }
    if (!found) lfm.off = lfm.cur = 0;
}

void move_right(void) {
    file_t file = lfm.files.buf[lfm.cur];
    if (file.type != T_DIR) return;
    char path[PATH_MAX] = {0};
    sprintf(path, "%s/%s", lfm.path, file.name);
    list_files(path);
}

void move_up(void) {
    if (lfm.cur == 0) return;
    --lfm.cur;
    scroll_up();
}

void move_down(void) {
    if (lfm.cur >= lfm.files.sz-1) return;
    ++lfm.cur;
    scroll_down();
}

void move_home(void) {
    lfm.cur = lfm.off = 0;
}

void move_end(void) {
    move_home();
    while (lfm.cur < lfm.files.sz-1) move_down();
}

void page_up(void) {
    for (int i = 0; i < lfm.wh-2; ++i) move_up();
}

void page_down(void) {
    for (int i = 0; i < lfm.wh-2; ++i) move_down();
}

void toggle_hidden(void) {
    char *file = strdup(lfm.files.buf[lfm.cur].name);
    lfm.hidden = !lfm.hidden;
    reload_files();
    if (lfm.files.sz && strcmp(lfm.files.buf[lfm.cur].name, file) != 0) {
        memcpy(lfm.input.text, file, (lfm.input.text_sz = strlen(file)));
        find_next();
    }
    free(file);
}

void find_next(void) {
    int found = 0, where = 0;
    char to_find[PATH_MAX] = {0};
    memcpy(to_find, lfm.input.text, lfm.input.text_sz);
    for (where = lfm.cur+1; where < lfm.files.sz; ++where) {
        if (strstr(lfm.files.buf[where].name, to_find)) {
            found = 1; break;
        }
    }
    if (!found) {
        for (where = 0; where < lfm.cur; ++where) {
            if (strstr(lfm.files.buf[where].name, to_find)) {
                found = 1; break;
            }
        }
    }
    if (found) for (lfm.cur = lfm.off = 0; lfm.cur < where; move_down());
}

void open_shell(void) {
    execute("$SHELL");
}

void edit_file(void) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "$EDITOR '%s'", lfm.files.buf[lfm.cur].name);
    execute(cmd);
}

void execute(char *cmd) {
    _quit_curses();
    system(cmd);
    reload_files();
    _init_curses();
}

static void _render_file(int l) {
    file_t file = lfm.files.buf[l];
    char *prefix = (_find_file(&lfm.selection, file) != -1)? SELECTED_PREFIX : "";
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
    mvprintw(l-lfm.off, 1, "%s%s%s", prefix, file.name, postfix);
    attroff(attr);
}

void render_files(void) {
    erase();
    if (lfm.files.sz == 0) {
        attron(A_REVERSE);
        mvprintw(0, 0, "  empty  ");
        attroff(A_REVERSE);
    }
    for (int i = lfm.off; i < lfm.off+lfm.wh-1; ++i) {
        if (i >= lfm.files.sz) break;
        _render_file(i);
    }
}

static char *action_to_cstr[] = {
    [ACTION_FIND] = "Find: ",
    [ACTION_EXEC] = "Exec: ",
    [ACTION_OPEN] = "Open: ",
    [ACTION_MOVE] = "Move: ",
    [ACTION_COPY] = "Copy: ",
    [ACTION_DELETE] = "Delete: ",
};

void render_status(void) {
    char status[ALLOC_SIZE] = {0};
    sprintf(status, " %d %d:%ld %s ", lfm.selection.sz, lfm.cur+1, lfm.files.sz, lfm.path);
    const size_t status_sz = strlen(status);
    attron(ATTR_STATUS);
    mvprintw(lfm.wh-1, lfm.ww-status_sz, status);
    if (lfm.action != ACTION_NONE) {
        char *astr = action_to_cstr[lfm.action];
        mvprintw(lfm.wh-1, 0, "%s", astr);
        input_render(&lfm.input, strlen(astr), lfm.wh-1, lfm.ww-status_sz-strlen(astr));
    }
    attroff(ATTR_STATUS);
}

static inline void _action_find(int ch) {
    if (lfm.input.text_sz) find_next();
}

static inline void _action_exec(int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "%.*s", lfm.input.text_sz, lfm.input.text);
    execute(cmd);
}

static inline void _action_open(int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "%.*s '%s'",
        lfm.input.text_sz, lfm.input.text, lfm.files.buf[lfm.cur].name);
    execute(cmd);
}

static inline void _action_move(int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "%s '%s' '%.*s'",
        (lfm.action == ACTION_MOVE)? "mv -f" : "cp -rf", lfm.files.buf[lfm.cur].name,
        lfm.input.text_sz, lfm.input.text);
    execute(cmd);
}

static inline void _action_delete(int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "rm -rf '%.*s'", lfm.input.text_sz, lfm.input.text);
    execute(cmd);
}

static inline void _execute_on_selection(char *op, bool to_path) {
    // XXX: ask for permission before performing action on selection
    const int sz = lfm.selection.sz*PATH_MAX*2;
    char *cmd = calloc(sz, sizeof(char));
    sprintf(cmd, "%s", op);
    for (int i = 0; i < lfm.selection.sz; ++i) {
        file_t *file = &lfm.selection.buf[i];
        sprintf(cmd, "%s '%s/%s'", cmd, file->path, file->name);
    }
    if (to_path) sprintf(cmd, "%s '%s'", cmd, lfm.path);
    execute(cmd);
    free(cmd);
}


static inline void _action_move_selected(void) {
    _execute_on_selection(lfm.action == ACTION_COPY? "cp -rf" : "mv -f", TRUE);
    lfm.selection.sz = 0;
}

static inline void _action_delete_selected(void) {
    _execute_on_selection("rm -rf", FALSE);
    lfm.selection.sz = 0;
}

static void (*sel_action_funs[NUM_ACTIONS])(void) = {
    [ACTION_MOVE] = _action_move_selected,
    [ACTION_COPY] = _action_move_selected,
    [ACTION_DELETE] = _action_delete_selected,
};

static void (*action_funs[NUM_ACTIONS])(int) = {
    [ACTION_FIND] = _action_find,
    [ACTION_EXEC] = _action_exec,
    [ACTION_OPEN] = _action_open,
    [ACTION_MOVE] = _action_move,
    [ACTION_COPY] = _action_move,
    [ACTION_DELETE] = _action_delete,
};

static inline void _update_action(int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        lfm.action = ACTION_NONE;
        return;
    }
    if (lfm.selection.sz && sel_action_funs[lfm.action]) {
        if (ch == '\n' || ch == 'y' || ch == 'Y') {
            sel_action_funs[lfm.action]();
            reload_files();
        }
        lfm.action = ACTION_NONE;
        return;
    } else if (ch == '\n') {
        action_funs[lfm.action](ch);
        reload_files();
        lfm.action = ACTION_NONE;
    }
    input_update(&lfm.input, ch);
}

void update(void) {
    int ch = getch();
    if (lfm.action != ACTION_NONE) return _update_action(ch);
    // XXX: allow for easily customizing keys
    switch (ch) {
    case 'q': case 'Q': case CTRL('q'):
        return quit_lfm();
    case CTRL('r'):
        reload_files();
        return;
    case CTRL('f'): case '/':
        lfm.action = ACTION_FIND;
        return input_reset(&lfm.input);
    case CTRL('n'): case 'n':
        if (lfm.input.text_sz) find_next();
        return;
    case ':':
        lfm.action = ACTION_EXEC;
        return input_reset(&lfm.input);
    case 'o': case 'O': case CTRL('o'):
        if (!lfm.files.sz) break;
        lfm.action = ACTION_OPEN;
        return input_reset(&lfm.input);
    case 'v': case 'V': case 'm': case 'M': case 'r': case 'R':
        if (!lfm.files.sz && !lfm.selection.sz) break;
        lfm.action = ACTION_MOVE;
        if (lfm.selection.sz) return input_set(&lfm.input, SELECTION_TEXT, strlen(SELECTION_TEXT));
        return input_set(&lfm.input, lfm.files.buf[lfm.cur].name, strlen(lfm.files.buf[lfm.cur].name));
    case 'c': case 'C':
        if (!lfm.files.sz && !lfm.selection.sz) break;
        lfm.action = ACTION_COPY;
        if (lfm.selection.sz) return input_set(&lfm.input, SELECTION_TEXT, strlen(SELECTION_TEXT));
        return input_set(&lfm.input, lfm.files.buf[lfm.cur].name, strlen(lfm.files.buf[lfm.cur].name));
    case 'd': case 'D': case 'x': case 'X':
        if (!lfm.files.sz && !lfm.selection.sz) break;
        lfm.action = ACTION_DELETE;
        if (lfm.selection.sz) return input_set(&lfm.input, SELECTION_TEXT, strlen(SELECTION_TEXT));
        return input_set(&lfm.input, lfm.files.buf[lfm.cur].name, strlen(lfm.files.buf[lfm.cur].name));
    case KEY_LEFT: case 'h':
        return move_left();
    case KEY_RIGHT: case 'l':
        return move_right();
    case KEY_UP: case 'k':
        return move_up();
    case KEY_DOWN: case 'j':
        return move_down();
    case KEY_HOME: case 'g':
        return move_home();
    case KEY_END: case 'G':
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
        if (!lfm.files.sz) break;
        return edit_file();
    case ' ':
        if (!lfm.files.sz) break;
        select_file(lfm.files.buf[lfm.cur]);
        if (lfm.cur+1 < lfm.files.sz) ++lfm.cur;
        break;
    case 'a': case 'A':
        return select_all_files(TRUE);
    case 'i': case 'I':
        return select_all_files(FALSE);
    case 'u': case 'U':
        lfm.selection.sz = 0;
        break;
    default: break;
    }
}

void main(int argc, char **argv) {
    init_lfm(argc < 2? "." : argv[1]);
    _init_curses();
    for (;;) {
        getmaxyx(stdscr, lfm.wh, lfm.ww);
        render_files();
        render_status();
        update();
    }
    quit_lfm();
}
