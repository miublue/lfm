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
    int c = fp->cur + dir, h = fp->wh-2;
    fp->cur = c<0? 0 : c>=fp->num_tabs? fp->num_tabs-1 : c;
    c=fp->cur, fp->off = c<fp->off? c : c-fp->off>=h? c-h : fp->off;
}

static void _picker_find_next(struct picker *fp, char *name) {
    int cur = fp->cur, off = fp->off, pos;
    for (pos = cur+1; pos < fp->num_tabs; ++pos)
        if (strcasestr(fp->tabs[pos], name)) goto j;
    for (pos = 0; pos < cur; ++pos)
        if (strcasestr(fp->tabs[pos], name)) goto j;
    fp->cur = cur, fp->off = off;
    return;
j:  fp->cur = fp->off = 0;
    _picker_move(fp, pos);
}

void picker_update(struct picker *fp, int ch) {
    if (fp->is_searching) {
        switch (ch) {
        case '\n': if (!fp->input.text_sz) break; _picker_find_next(fp, fp->input.text); /* FALLTHROUGH */
        case CTRL('q'): case CTRL('c'): fp->is_searching = 0; break;
        default: input_update(&fp->input, ch); break;
        }
        return;
    }
    switch (ch) {
    case KEY_NAVUP:   case KEY_UP:    _picker_move(fp, -1); break;
    case KEY_NAVDOWN: case KEY_DOWN:  _picker_move(fp, +1); break;
    case KEY_NAVTOP:  case KEY_HOME:  _picker_move(fp, -fp->num_tabs); break;
    case KEY_NAVEND:  case KEY_END:   _picker_move(fp, +fp->num_tabs); break;
    case KEY_PPAGE:                   _picker_move(fp, -(fp->wh-2)); break;
    case KEY_NPAGE:                   _picker_move(fp, +(fp->wh-2)); break;
    case KEY_MODE_FIND:
        input_reset(&fp->input);
        fp->is_searching = 1;
        break;
    case KEY_FIND_NEXT:
        if (fp->input.text_sz) _picker_find_next(fp, fp->input.text);
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
    const int attr = ATTR_STATUS | (_USE_COLOR? COLOR_PAIR(COLOR_STATUS) : 0);
    char status[ALLOC_SIZE] = {0};
    memset(status, ' ', fp->ww);
    attron(attr);
    mvprintw(fp->wh-1, 0, "%s", status);
    sprintf(status, "%d:%d *TABS* ", fp->cur+1, fp->num_tabs);
    mvprintw(fp->wh-1, fp->ww-strlen(status), "%s", status);
    if (fp->is_searching) {
        const char *str = "Find: ";
        const int cap = strlen(str)+strlen(status), inp_attr = attr & A_REVERSE? A_NORMAL : A_REVERSE;
        const int w = cap+5 > fp->ww? fp->ww-((int)strlen(str)) : fp->ww-cap;
        mvprintw(fp->wh-1, 0, "%s", str);
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

