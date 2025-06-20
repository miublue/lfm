#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "inputbox.h"

static inline void _input_insert_char(inputbox_t *ib, int c) {
    ib->text_sz++;
    memmove(ib->text+ib->pos+1, ib->text+ib->pos, ib->text_sz-ib->pos);
    ib->text[ib->pos++] = c;
}

static inline void _input_remove_char(inputbox_t *ib, bool backspace) {
    if (backspace) {
        if (ib->pos == 0) return;
        --ib->pos;
    }
    memmove(ib->text+ib->pos, ib->text+ib->pos+1, ib->text_sz-ib->pos);
    ib->text_sz--;
}

static inline bool _input_isdelim(char c) {
    return isspace(c) || !(isalnum(c) || c == '_');
}

static inline void _input_next_word(inputbox_t *ib) {
    if (ib->pos+1 >= ib->text_sz) return;
    if (_input_isdelim(ib->text[ib->pos+1]))
        while (ib->pos < ib->text_sz-1 && _input_isdelim(ib->text[ib->pos+1])) ++ib->pos;
    else while (ib->pos < ib->text_sz-1 && !_input_isdelim(ib->text[ib->pos+1])) ++ib->pos;
}

static inline void _input_prev_word(inputbox_t *ib) {
    if (ib->pos <= 1) return;
    if (_input_isdelim(ib->text[ib->pos-1]))
        while (ib->pos > 0 && _input_isdelim(ib->text[ib->pos-1])) --ib->pos;
    else while (ib->pos > 0 && !_input_isdelim(ib->text[ib->pos-1])) --ib->pos;
}

static inline void _input_remove_next_word(inputbox_t *ib) {
    if (ib->pos >= ib->text_sz) return;
    int p = ib->pos;
    _input_next_word(ib);
    for (; ib->pos > p && ib->pos > 0; --ib->pos) _input_remove_char(ib, FALSE);
    _input_remove_char(ib, FALSE);
}

static inline void _input_remove_prev_word(inputbox_t *ib) {
    if (ib->pos == 0) return;
    int p = ib->pos;
    _input_prev_word(ib);
    for (; ib->pos <= p && ib->pos < ib->text_sz; --p) _input_remove_char(ib, FALSE);
}

void input_update(inputbox_t *ib, int key) {
    switch (key) {
#ifdef _USE_MTM /* since i sometimes use the mtm terminal multiplexer */
    case 200: {
        switch (getch()) {
        case 170: _input_prev_word(ib); break;
        case 185: _input_next_word(ib); break;
        case 144: _input_remove_next_word(ib); break;
        default: break;
        }
    } break;
#endif
    case KEY_LEFT:
        if (ib->pos > 0) --ib->pos;
        break;
    case KEY_RIGHT:
        if (ib->pos < ib->text_sz) ++ib->pos;
        break;
    case 545: case 554: case 557: // ctrl + left
        _input_prev_word(ib);
        break;
    case 560: case 569: case 572: // ctrl + right
        _input_next_word(ib);
        break;
    case KEY_UP: case KEY_HOME:
        ib->pos = 0;
        break;
    case KEY_DOWN: case KEY_END:
        ib->pos = ib->text_sz;
        break;
    case KEY_DC:
        if (ib->pos >= ib->text_sz) break;
        _input_remove_char(ib, FALSE);
        break;
    case KEY_BACKSPACE:
        _input_remove_char(ib, TRUE);
        break;
    case 528: case 531: // ctrl + del
        _input_remove_next_word(ib);
        break;
    case 127: case 8: // ctrl + backspace
        _input_remove_prev_word(ib);
        break;
    default:
        if (isprint(key)) _input_insert_char(ib, key);
        break;
    }
}

void input_render(inputbox_t *ib, int x, int y, int w) {
    char text[INPUTBOX_TEXT_SIZE] = {0};
    memset(text, ' ', sizeof(text));
    memcpy(text, ib->text, MIN(ib->text_sz, w));
    mvprintw(y, x, "%.*s", w, text);
    attron(A_REVERSE);
    mvprintw(y, x+ib->pos, "%c", isprint(ib->text[ib->pos])? ib->text[ib->pos] : ' ');
    attroff(A_REVERSE);
}

void input_reset(inputbox_t *ib) {
    ib->text_sz = ib->pos = 0;
    memset(ib->text, 0, sizeof(ib->text));
}

void input_set(inputbox_t *ib, char *text, int sz) {
    input_reset(ib);
    int _sz = MIN(INPUTBOX_TEXT_SIZE, sz);
    ib->text_sz = ib->pos = _sz;
    memcpy(ib->text, text, _sz);
}
