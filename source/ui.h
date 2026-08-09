#ifndef UI_H
#define UI_H

#include <stdbool.h>

enum {
    SCHERMO_TITOLO = 0,
    SCHERMO_GIOCA,
    SCHERMO_SCELTA,
    SCHERMO_RIEPILOGO,
    SCHERMO_FINALE,
    SCHERMO_REGOLE,
    SCHERMO_WIFI,
    SCHERMO_WIFI_ATT,
};

void ui_init(int bg_top, int bg_sub, int test_ok, int test_fail);
void ui_loop(void);

#endif
