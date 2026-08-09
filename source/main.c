/* ============================================================
   L'ISOLA - Sopravvivenza (NDS Edition)
   Punto di ingresso: init libnds, autoverifica, loop principale.
   ============================================================ */

#include <nds.h>
#include "game.h"
#include "ui.h"
#include "save.h"

int main(void) {
    int bg_top, bg_sub, ok, fail;

    defaultExceptionHandler();

    /* due schermi in modalita bitmap 16 bit (esempio ufficiale 16bit_color_bmp) */
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);

    bg_top = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bg_sub = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    save_init();

    soundEnable();

    /* autoverifica (equivalente di index.html?test) */
    g_test_automatici(&ok, &fail);

    ui_init(bg_top, bg_sub, ok, fail);

    while (1) {
        swiWaitForVBlank();
        ui_loop();
    }
}
