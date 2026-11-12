#ifndef HOME_WELCOME_H
#define HOME_WELCOME_H

#include "home-svg.h"

typedef struct {
    const char *language;
    xmlDocPtr document;
    double first, length;
} Welcome;

Welcome welcome_load(const char *language, const char *path);
xmlNodePtr welcome_render(const Welcome *welcome, xmlNodePtr group, double offset,
                          double total, int mobile);

#endif
