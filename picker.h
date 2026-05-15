#ifndef __PICKER_H
#define __PICKER_H

#include <dirent.h>
#include <stdlib.h>
#include "lfm.h"
#include "config.h"

#ifndef PICKER_TABS_MAX
#define PICKER_TABS_MAX 1024
#endif

struct picker {
    char *tabs[PICKER_TABS_MAX];
    int num_tabs, cur, off, ww, wh, is_searching;
    struct inputbox input;
};

void picker_update(struct picker *fp, int ch);
void picker_render(struct picker *fp);
void picker_reset(struct picker *fp);

static void _picker_move(struct picker *fp, int dir) {
    fp->cur += dir;
    if (fp->cur < 0) fp->cur = 0;
    if (fp->cur >= fp->num_tabs) fp->cur = fp->num_tabs-1;
    if (fp->cur < fp->off) --fp->off;
    if (fp->wh != 0 && fp->cur-fp->off >= fp->wh-1) ++fp->off;
}

static void _picker_find(struct picker *fp, char *name) {
    int cur = fp->cur, off = fp->off, pos, found = 0;
    for (pos = cur+1; pos < fp->num_tabs; ++pos)
        if (strcasestr(fp->tabs[pos], name)) goto jmp;
    for (pos = 0; pos < cur; ++pos)
        if (strcasestr(fp->tabs[pos], name)) goto jmp;
    fp->cur = cur, fp->off = off;
    return;
jmp:
    for (fp->cur = fp->off = 0; fp->cur < pos; _picker_move(fp, 1)){}
}

static void _picker_find_next(struct picker *fp) {
    if (!fp->input.text_sz) return;
    char *name = strndup(fp->input.text, fp->input.text_sz);
    _picker_find(fp, name);
    free(name);
}

static void _picker_update_find(struct picker *fp, int ch) {
    if (ch == CTRL('q') || ch == CTRL('c')) {
        fp->is_searching = 0;
        return;
    }
    if ((ch == CTRL('f') || ch == '\n') && fp->input.text_sz) {
        _picker_find_next(fp);
        fp->is_searching = 0;
    } else input_update(&fp->input, ch);
}

void picker_update(struct picker *fp, int ch) {
    if (fp->is_searching) return _picker_update_find(fp, ch);
    switch (ch) {
    case KEY_UP:
        _picker_move(fp, -1);
        break;
    case KEY_DOWN:
        _picker_move(fp, 1);
        break;
    case KEY_PPAGE:
        for (int i = 0; i < fp->wh-1; ++i) _picker_move(fp, -1);
        break;
    case KEY_NPAGE:
        for (int i = 0; i < fp->wh-1; ++i) _picker_move(fp, 1);
        break;
    case KEY_HOME:
        fp->cur = fp->off = 0;
        break;
    case KEY_END:
        for (fp->cur = fp->off = 0; fp->cur+1 < fp->num_tabs; ) _picker_move(fp, 1);
        break;
    case CTRL('f'):
        input_reset(&fp->input);
        fp->is_searching = 1;
        break;
    case CTRL('n'): case 'n':
        if (fp->input.text_sz) _picker_find_next(fp);
        break;
    }
}

void picker_render(struct picker *fp) {
    getmaxyx(stdscr, fp->wh, fp->ww);
    for (int i = fp->off; i < fp->off+fp->wh; ++i) {
        if (i >= fp->num_tabs) break;
        const int attr = i == fp->cur? A_REVERSE : 0;
        attron(attr);
        char *name = expand_home(fp->tabs[i]);
        mvprintw(i-fp->off, 0, "%.*s/", fp->ww, name);
        free(name);
        attroff(attr);
    }
#if _USE_COLOR
    const int attr = ATTR_STATUS|COLOR_PAIR(COLOR_STATUS);
#else
    const int attr = ATTR_STATUS;
#endif
    char status[ALLOC_SIZE] = {0};
    memset(status, ' ', fp->ww);
    attron(attr);
    mvprintw(fp->wh-1, 0, "%s", status);
    sprintf(status, "%d:%d *TABS* ", fp->cur+1, fp->num_tabs);
    mvprintw(fp->wh-1, fp->ww-strlen(status), "%s", status);
    if (fp->is_searching) {
        const char *str = "Find: ";
        const int cap = strlen(str)+strlen(status), inp_attr = attr & A_REVERSE? A_NORMAL : A_REVERSE;
        const int w = cap+5 > fp->ww? fp->ww-strlen(str) : fp->ww-cap;
        mvprintw(fp->wh-1, 0, str);
        input_render(&fp->input, strlen(str), fp->wh-1, w, inp_attr);
    }
    attroff(attr);
}

void picker_reset(struct picker *fp) {
    if (fp->num_tabs) {
        for (int i = 0; i < fp->num_tabs; ++i)
            free(fp->tabs[i]);
    }
    fp->cur = fp->off = fp->num_tabs = fp->is_searching = 0;
    input_reset(&fp->input);
}

#endif
