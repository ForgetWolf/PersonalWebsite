#ifndef HOME_TERMINAL_H
#define HOME_TERMINAL_H

#include "home-svg.h"

typedef struct TerminalTrace TerminalTrace;
TerminalTrace *terminal_execute(void);
void terminal_render(xmlNodePtr root, const TerminalTrace *trace, int mobile);
void terminal_free(TerminalTrace *trace);

#endif
