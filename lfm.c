#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ncurses.h>
#include "lfm.h"
#define INPUTBOX_IMPL
#include "inputbox.h"

static struct {
    int show_hidden;
} opts;

static struct {
    const char *prgname;
    struct files_list selection;
    int action, ww, wh, num_tabs, max_tabs;
    struct tab *tabs, *cur_tab;
    struct inputbox input;
} lfm;

static void _init_curses(void) {
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    define_key("\e[1~", KEY_HOME);
    define_key("\e[4~", KEY_END);
#ifdef _USE_MTM
    char key[10] = {0};
    sprintf(key, "%c%c", 200, 144); define_key(key, 528); // kDC5
    sprintf(key, "%c%c", 200, 170); define_key(key, 554); // kLFT5
    sprintf(key, "%c%c", 200, 185); define_key(key, 569); // kRIT5
#endif
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

static void _init_files(struct files_list *list) {
    list->buf = malloc(sizeof(struct file) * (list->cap = ALLOC_SIZE));
    list->sz = 0;
}

static void _free_files(struct files_list *list) {
    free(list->buf);
    list->sz = list->cap = 0;
}

static void _append_file(struct files_list *list, struct file file) {
    if (list->sz >= list->cap)
        list->buf = realloc(list->buf, sizeof(struct file) * (list->cap += ALLOC_SIZE));
    list->buf[list->sz++] = file;
}

// return index of file if selected, else return -1
static int _find_file(struct files_list *list, struct file file) {
    for (int i = 0; i < list->sz; ++i) {
        struct file *f = &list->buf[i];
        if (!strcmp(f->path, file.path) && !strcmp(f->name, file.name)) return i;
    }
    return -1;
}

static void _remove_file(struct files_list *list, int idx) {
    if (list->sz == 0 || idx >= list->sz) return;
    --list->sz;
    for (int i = idx; i < list->sz; ++i) list->buf[i] = list->buf[i+1];
}

struct tab *create_tab(char *path) {
    if (lfm.num_tabs >= lfm.max_tabs)
        lfm.tabs = realloc(lfm.tabs, (lfm.max_tabs *= 1.5)*sizeof(struct tab));
    struct tab *tab = &lfm.tabs[lfm.num_tabs++];
    tab->path = NULL;
    tab->cur = tab->off = 0;
    tab->show_hidden = opts.show_hidden;
    _init_files(&tab->files);
    list_files(tab, path);
    return tab;
}

void close_tab(struct tab *tab) {
    if (--lfm.num_tabs == 0) quit_lfm(tab->path);
    if (tab->path) free(tab->path);
    if (tab->files.cap) _free_files(&tab->files);
    lfm.action = ACTION_NONE;
    for (struct tab *t = tab; t != &lfm.tabs[lfm.num_tabs]; ++t) *t = *(t+1);
    if (lfm.cur_tab == &lfm.tabs[lfm.num_tabs]) --lfm.cur_tab;
}

static void _switch_tab(void) {
    if (lfm.num_tabs > 1) {
        if (++lfm.cur_tab == &lfm.tabs[lfm.num_tabs])
            lfm.cur_tab = &lfm.tabs[0];
        reload_files(lfm.cur_tab);
    }
}

void init_lfm(char *path) {
    lfm.action = ACTION_NONE;
    lfm.tabs = malloc((lfm.max_tabs = ALLOC_SIZE) * sizeof(struct tab));
    lfm.num_tabs = 0;
    _init_files(&lfm.selection);
    lfm.cur_tab = create_tab(path);
    input_reset(&lfm.input);
}

void quit_lfm(char *path) {
    FILE *file = fopen("/tmp/lfmsel", "w");
    fwrite(path, strlen(path), 1, file);
    fclose(file);
    _quit_curses();
    exit(0);
}

static struct file _stat_file(struct tab *tab, char *name) {
    char path[PATH_MAX] = {0};
    sprintf(path, "%s/%s", tab->path, name);
    struct stat file_stat;
    stat(path, &file_stat);
    // XXX: make some amalgamation of stat and lstat
    //      checking S_ISLNK(st_mode) and st_nlink > 1
    struct file file = {
        .is_link = S_ISLNK(file_stat.st_mode),
        .name = {0},
        .path = {0},
    };
    file.type = S_ISDIR(file_stat.st_mode)? T_DIR : (file_stat.st_mode & S_IXUSR)? T_EXEC : T_FILE;
    memcpy(file.name, name, strlen(name));
    memcpy(file.path, tab->path, strlen(tab->path));
    return file;
}

static int _compare_files(const void *a_ptr, const void *b_ptr) {
    const struct file *a = (struct file*)a_ptr, *b = (struct file*)b_ptr;
    if (a->type == T_DIR && b->type != T_DIR) return -1;
    if (a->type != T_DIR && b->type == T_DIR) return 1;
    else return (strcasecmp(a->name, b->name));
}

void list_files(struct tab *tab, char *path) {
    if (tab->path) free(tab->path);
    tab->path = realpath(path, NULL);
    if (!tab->path) goto fail;
    chdir(tab->path);
    tab->files.sz = tab->cur = tab->off = 0;
    // XXX: i could probably use scandir with alphasort also
    struct dirent *ent = NULL;
    DIR *dir = opendir(path);
    if (!dir) goto fail;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (!tab->show_hidden && ent->d_name[0] == '.') continue;
        struct file file = _stat_file(tab, ent->d_name);
        _append_file(&tab->files, file);
    }

    qsort(tab->files.buf, tab->files.sz, sizeof(struct file), &_compare_files);
    closedir(dir);
    return;
fail:
    // XXX: maybe try displaying error on statusbar and chdir
    //      to home directory before giving up and exitting.
    fprintf(stderr, "error: path '%s' does not exist\n", path);
    exit(1);
}

void select_file(struct file file) {
    int idx = _find_file(&lfm.selection, file);
    if (idx != -1) _remove_file(&lfm.selection, idx);
    else _append_file(&lfm.selection, file);
}

void select_all_files(struct tab *tab, bool add_all) {
    for (int i = 0; i < tab->files.sz; ++i) {
        if (_find_file(&lfm.selection, tab->files.buf[i]) == -1 || !add_all) {
            select_file(tab->files.buf[i]);
        }
    }
}

void reload_files(struct tab *tab) {
    char path[PATH_MAX] = {0};
    int cur = tab->cur, off = tab->off;
    sprintf(path, "%s", tab->path);
    list_files(tab, path);
    if (!tab->files.sz) {
        tab->cur = tab->off = 0;
        return;
    }
    tab->cur = cur; tab->off = off;
    while (tab->cur >= 0 && tab->cur >= tab->files.sz) move_up(tab);
}

void scroll_up(struct tab *tab) {
    if (tab->cur-tab->off < 0 && tab->off > 0) --tab->off;
}

void scroll_down(struct tab *tab) {
    if (tab->cur-tab->off > lfm.wh-2) ++tab->off;
}

void move_left(struct tab *tab) {
    char prev[PATH_MAX] = {0}, path[PATH_MAX] = {0}, found = 0;
    int sz = strlen(tab->path)-1;
    for (; sz >= 0 && tab->path[sz] != '/'; --sz);
    memcpy(prev, tab->path+sz+1, strlen(tab->path)-sz);
    sprintf(path, "%s/..", tab->path);
    list_files(tab, path);
    for (int i = 0; i < tab->files.sz; ++i) {
        if (!strcmp(tab->files.buf[i].name, prev)) {
            found = 1;
            break;
        }
        move_down(tab);
    }
    if (!found) tab->off = tab->cur = 0;
}

void move_right(struct tab *tab) {
    if (!tab->files.sz) return;
    struct file file = tab->files.buf[tab->cur];
    if (file.type != T_DIR) return;
    char path[PATH_MAX] = {0};
    sprintf(path, "%s/%s", tab->path, file.name);
    list_files(tab, path);
}

void move_up(struct tab *tab) {
    if (tab->cur == 0 || !tab->files.sz) return;
    --tab->cur;
    scroll_up(tab);
}

void move_down(struct tab *tab) {
    if (tab->cur >= tab->files.sz-1 || !tab->files.sz) return;
    ++tab->cur;
    scroll_down(tab);
}

void move_home(struct tab *tab) {
    tab->cur = tab->off = 0;
}

void move_end(struct tab *tab) {
    move_home(tab);
    while (tab->cur < tab->files.sz-1) move_down(tab);
}

void page_up(struct tab *tab) {
    for (int i = 0; i < lfm.wh-2; ++i) move_up(tab);
}

void page_down(struct tab *tab) {
    for (int i = 0; i < lfm.wh-2; ++i) move_down(tab);
}

void toggle_hidden(struct tab *tab) {
    char *file = strdup(tab->files.buf[tab->cur].name);
    tab->show_hidden = !tab->show_hidden;
    reload_files(tab);
    if (tab->files.sz && strcmp(tab->files.buf[tab->cur].name, file) != 0)
        find_next(tab, file, strlen(file));
    free(file);
}

// XXX: allow for searching only directories or files
void find_next(struct tab *tab, char *str, int sz) {
    if (!tab->files.sz) return;
    char to_find[PATH_MAX] = {0};
    memcpy(to_find, str, sz);
    int found = 0, where = 0;
    for (where = tab->cur+1; where < tab->files.sz; ++where) {
        if (strcasestr(tab->files.buf[where].name, to_find)) {
            found = 1; break;
        }
    }
    if (!found) {
        for (where = 0; where < tab->cur; ++where) {
            if (strcasestr(tab->files.buf[where].name, to_find)) {
                found = 1; break;
            }
        }
    }
    if (found) for (tab->cur = tab->off = 0; tab->cur < where; move_down(tab));
}

void open_shell(struct tab *tab) {
    execute(tab, "$SHELL");
}

void edit_file(struct tab *tab) {
    if (!tab->files.sz) return;
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "$EDITOR '%s'", tab->files.buf[tab->cur].name);
    execute(tab, cmd);
}

void execute(struct tab *tab, char *cmd) {
    _quit_curses();
    system(cmd);
    reload_files(tab);
    _init_curses();
}

static void _render_file(struct tab *tab, int l) {
    struct file file = tab->files.buf[l];
    char *prefix = (_find_file(&lfm.selection, file) != -1)? SELECTION_PREFIX : "";
    char postfix[3] = {0};
    int attr = 0, affix_size, size;

    switch (file.type) {
    case T_DIR:  attr = ATTR_DIR|COLOR_PAIR(PAIR_DIR);   strcat(postfix, "/"); break;
    case T_EXEC: attr = ATTR_EXEC|COLOR_PAIR(PAIR_EXEC); strcat(postfix, "*"); break;
    default:     attr = ATTR_FILE|COLOR_PAIR(PAIR_NORMAL); break;
    }
    if (file.is_link) { attr = ATTR_LINK|COLOR_PAIR(PAIR_LINK); strcat(postfix, "@"); }
    if (tab->cur == l) attr |= A_REVERSE;

    affix_size = strlen(prefix) + strlen(postfix);
    size = MIN(lfm.ww-affix_size, strlen(file.name));
    attron(attr);
    mvprintw(l-tab->off, 0, " %s%.*s%s", prefix, size, file.name, postfix);
    attroff(attr);
}

void render_files(struct tab *tab) {
    erase();
    if (tab->files.sz == 0) {
        attron(A_REVERSE);
        mvprintw(0, 0, "  empty  ");
        attroff(A_REVERSE);
    }
    for (int i = tab->off; i < tab->off+lfm.wh-1; ++i) {
        if (i >= tab->files.sz) break;
        _render_file(tab, i);
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

static char *_expand_home(struct tab *tab) {
#if EXPAND_HOME
    const char *home = getenv("HOME");
    if (!strstr(tab->path, home)) goto end;
    char *path = malloc(PATH_MAX);
    memset(path, 0, PATH_MAX);
    path[0] = '~';
    strcat(path, tab->path+strlen(home));
    return path;
#endif
end:
    return strdup(tab->path);
}

void render_status(void) {
#if _USE_COLOR
    const int attr = ATTR_STATUS|COLOR_PAIR(COLOR_STATUS);
#else
    const int attr = ATTR_STATUS;
#endif
    struct tab *tab = lfm.cur_tab;
    char status[ALLOC_SIZE] = {0};
    memset(status, ' ', lfm.ww);
    attron(attr);
    mvprintw(lfm.wh-1, 0, "%s", status);
    char *path = _expand_home(tab);
    sprintf(status, " %ld %d:%ld (%d:%d %s) ",
        lfm.selection.sz, tab->cur+1, tab->files.sz,
        (tab-lfm.tabs)+1, lfm.num_tabs, path);
    free(path);
    const size_t status_sz = strlen(status);
    mvprintw(lfm.wh-1, lfm.ww-status_sz, "%s", status);
    if (lfm.action != ACTION_NONE) {
        char *astr = action_to_cstr[lfm.action];
        mvprintw(lfm.wh-1, 0, "%s", astr);
        const int input_attr = attr & A_REVERSE? A_NORMAL : A_REVERSE;
        const int sz = strlen(astr)+strlen(status);
        const int cap = sz+5 > lfm.ww? lfm.ww-strlen(astr) : lfm.ww-sz;
        input_render(&lfm.input, strlen(astr), lfm.wh-1, cap, input_attr);
    }
    attroff(attr);
}

static inline void _action_find(struct tab *tab, int ch) {
    if (lfm.input.text_sz) find_next(tab, lfm.input.text, lfm.input.text_sz);
}

static inline void _action_exec(struct tab *tab, int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "%.*s", lfm.input.text_sz, lfm.input.text);
    execute(tab, cmd);
}

static inline void _action_open(struct tab *tab, int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "%.*s '%s'",
        lfm.input.text_sz, lfm.input.text, tab->files.buf[tab->cur].name);
    execute(tab, cmd);
}

static inline void _action_move(struct tab *tab, int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "%s '%s' '%.*s'", (lfm.action == ACTION_MOVE)? "mv -f" : "cp -rf",
        tab->files.buf[tab->cur].name, lfm.input.text_sz, lfm.input.text);
    execute(tab, cmd);
}

static inline void _action_delete(struct tab *tab, int ch) {
    char cmd[ALLOC_SIZE] = {0};
    sprintf(cmd, "rm -rf '%.*s'", lfm.input.text_sz, lfm.input.text);
    execute(tab, cmd);
}

static inline void _execute_on_selection(struct tab *tab, char *op, bool to_path) {
    const int sz = lfm.selection.sz*PATH_MAX*2;
    char *cmd = calloc(sz, sizeof(char));
    sprintf(cmd, "%s", op);
    for (int i = 0; i < lfm.selection.sz; ++i) {
        struct file *file = &lfm.selection.buf[i];
        sprintf(cmd, "%s '%s/%s'", cmd, file->path, file->name);
    }
    if (to_path) sprintf(cmd, "%s '%s'", cmd, tab->path);
    execute(tab, cmd);
    free(cmd);
}

static inline void _action_move_selected(struct tab *tab) {
    _execute_on_selection(tab, lfm.action == ACTION_COPY? "cp -rf" : "mv -f", TRUE);
    lfm.selection.sz = 0;
}

static inline void _action_delete_selected(struct tab *tab) {
    _execute_on_selection(tab, "rm -rf", FALSE);
    lfm.selection.sz = 0;
}

static void (*sel_action_funs[NUM_ACTIONS])(struct tab*) = {
    [ACTION_MOVE] = _action_move_selected,
    [ACTION_COPY] = _action_move_selected,
    [ACTION_DELETE] = _action_delete_selected,
};

static void (*action_funs[NUM_ACTIONS])(struct tab*, int) = {
    [ACTION_FIND] = _action_find,
    [ACTION_EXEC] = _action_exec,
    [ACTION_OPEN] = _action_open,
    [ACTION_MOVE] = _action_move,
    [ACTION_COPY] = _action_move,
    [ACTION_DELETE] = _action_delete,
};

static inline void _update_action(struct tab *tab, int ch) {
    if (ch == CTRL('c') || ch == CTRL('q')) {
        lfm.action = ACTION_NONE;
        return;
    }
    if (lfm.selection.sz && sel_action_funs[lfm.action]) {
        if (ch == '\n' || ch == 'y' || ch == 'Y') {
            sel_action_funs[lfm.action](tab);
            reload_files(tab);
        }
        lfm.action = ACTION_NONE;
        return;
    } else if (ch == '\n') {
        action_funs[lfm.action](tab, ch);
        reload_files(tab);
        lfm.action = ACTION_NONE;
    }
    input_update(&lfm.input, ch);
}

static inline void _get_term_size(void) {
    getmaxyx(stdscr, lfm.wh, lfm.ww);
    if (lfm.ww < MIN_TERM_WIDTH || lfm.wh < MIN_TERM_HEIGHT) {
        _quit_curses();
        fprintf(stderr, "error: %s requires minimal terminal size of %dx%d.\n",
                lfm.prgname, MIN_TERM_WIDTH, MIN_TERM_HEIGHT);
        exit(1);
    }
}

void update(struct tab *tab) {
    int ch = getch();
    if (ch == KEY_RESIZE) {
        _get_term_size();
        while (tab->cur-tab->off < 0) move_down(tab);
        while (tab->cur-tab->off >= lfm.wh-1) move_up(tab);
        return;
    }
    if (lfm.action != ACTION_NONE) return _update_action(tab, ch);
    switch (ch) {
    case CTRL('q'): case KEY_QUIT:
        return close_tab(tab);
    case CTRL('r'): case KEY_RELOAD:
        reload_files(tab);
        return;
    case KEY_MODE_FIND:
        lfm.action = ACTION_FIND;
        return input_reset(&lfm.input);
    case KEY_FIND_NEXT:
        if (lfm.input.text_sz) find_next(tab, lfm.input.text, lfm.input.text_sz);
        return;
    case KEY_MODE_EXEC:
        lfm.action = ACTION_EXEC;
        return input_reset(&lfm.input);
    case KEY_MODE_OPEN:
        if (!tab->files.sz) break;
        lfm.action = ACTION_OPEN;
        return input_reset(&lfm.input);
    case KEY_MODE_MOVE:
        if (!tab->files.sz && !lfm.selection.sz) break;
        lfm.action = ACTION_MOVE;
        if (lfm.selection.sz) return input_set(&lfm.input, SELECTION_TEXT, strlen(SELECTION_TEXT));
        return input_set(&lfm.input, tab->files.buf[tab->cur].name, strlen(tab->files.buf[tab->cur].name));
    case KEY_MODE_COPY:
        if (!tab->files.sz && !lfm.selection.sz) break;
        lfm.action = ACTION_COPY;
        if (lfm.selection.sz) return input_set(&lfm.input, SELECTION_TEXT, strlen(SELECTION_TEXT));
        return input_set(&lfm.input, tab->files.buf[tab->cur].name, strlen(tab->files.buf[tab->cur].name));
    case KEY_MODE_DELETE:
        if (!tab->files.sz && !lfm.selection.sz) break;
        lfm.action = ACTION_DELETE;
        if (lfm.selection.sz) return input_set(&lfm.input, SELECTION_TEXT, strlen(SELECTION_TEXT));
        return input_set(&lfm.input, tab->files.buf[tab->cur].name, strlen(tab->files.buf[tab->cur].name));
    case KEY_LEFT: case KEY_NAVBACK:
        return move_left(tab);
    case KEY_RIGHT: case KEY_NAVNEXT:
        return move_right(tab);
    case KEY_UP: case KEY_NAVUP:
        return move_up(tab);
    case KEY_DOWN: case KEY_NAVDOWN:
        return move_down(tab);
    case KEY_HOME: case KEY_NAVTOP:
        return move_home(tab);
    case KEY_END: case KEY_NAVEND:
        return move_end(tab);
    case KEY_PPAGE:
        return page_up(tab);
    case KEY_NPAGE:
        return page_down(tab);
    case KEY_SHOW_HIDDEN:
        return toggle_hidden(tab);
    case KEY_NEW_TAB:
        lfm.cur_tab = create_tab(lfm.cur_tab->path);
        break;
    case  KEY_NEXT_TAB:
        return _switch_tab();
    case KEY_SHELL:
        return open_shell(tab);
    case KEY_EDIT_FILE:
        if (!tab->files.sz) break;
        return edit_file(tab);
    case KEY_SELECT_FILE:
        if (!tab->files.sz) break;
        select_file(tab->files.buf[tab->cur]);
        move_down(tab);
        break;
    case KEY_SELECT_ALL:
        return select_all_files(tab, TRUE);
    case KEY_SELECT_INVERT:
        return select_all_files(tab, FALSE);
    case KEY_SELECT_EMPTY:
        lfm.selection.sz = 0;
        break;
    default: break;
    }
}

static inline void _usage(void) {
    fprintf(stderr, "usage: %s [-h|-x] [path]\n", lfm.prgname);
    fprintf(stderr, "    -h    show this help and exit\n");
    fprintf(stderr, "    -x    show hidden files by default\n");
    exit(0);
}

int main(int argc, char **argv) {
    lfm.prgname = argv[0];
    char *path = ".";
    opts.show_hidden = SHOW_HIDDEN;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h")) _usage();
        else if (!strcmp(argv[i], "-x")) opts.show_hidden = TRUE;
        else path = argv[i];
    }
    init_lfm(path);
    _init_curses();
    _get_term_size();
    for (;;) {
        render_files(lfm.cur_tab);
        render_status();
        update(lfm.cur_tab);
    }
    quit_lfm(lfm.cur_tab->path);
    return 0;
}
