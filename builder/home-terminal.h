#ifndef HOME_TERMINAL_H
#define HOME_TERMINAL_H

#include "home-svg.h"
#include "terminal-css.h"

typedef struct TerminalTrace TerminalTrace;
TerminalTrace *terminal_execute(void);
void terminal_render(xmlNodePtr root, const TerminalTrace *trace, bool mobile, TerminalCss &css);
void terminal_free(TerminalTrace *trace);

#endif
