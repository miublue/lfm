#ifndef __CONFIG_H
#define __CONFIG_H

#define SELECTION_PREFIX " + "
#define SELECTION_TEXT   "selection"

#define SHOW_HIDDEN 0

#define COLOR_STATUS COLOR_RED
#define COLOR_DIR    COLOR_BLUE
#define COLOR_EXEC   COLOR_GREEN
#define COLOR_LINK   COLOR_YELLOW

#define ATTR_DIR    A_BOLD
#define ATTR_EXEC   A_BOLD
#define ATTR_LINK   0
#define ATTR_FILE   0
#define ATTR_STATUS A_BOLD

#define KEY_QUIT   'q'
#define KEY_RELOAD 'r'
#define KEY_SHELL  's'

#define KEY_SELECT_FILE   ' '
#define KEY_SELECT_ALL    'a'
#define KEY_SELECT_INVERT 'i'
#define KEY_SELECT_EMPTY  'u'

#define KEY_SHOW_HIDDEN '.'

#define KEY_MODE_DELETE 'd'
#define KEY_MODE_OPEN   'o'
#define KEY_MODE_EXEC   ':'
#define KEY_MODE_MOVE   'm'
#define KEY_MODE_COPY   'c'
#define KEY_MODE_FIND   'f'
#define KEY_FIND_NEXT   'n'
#define KEY_EDIT_FILE   'e'

#define KEY_NAVUP   'k'
#define KEY_NAVDOWN 'j'
#define KEY_NAVTOP  'g'
#define KEY_NAVEND  'G'
#define KEY_NAVBACK 'h'
#define KEY_NAVNEXT 'l'

#endif
