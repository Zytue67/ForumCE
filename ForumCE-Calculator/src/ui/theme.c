#include <graphx.h>

#include "theme.h"

void theme_init(void)
{
    gfx_palette[COLOR_BACKGROUND] =
        gfx_RGBTo1555(15, 23, 42);

    gfx_palette[COLOR_PANEL] =
        gfx_RGBTo1555(31, 41, 55);

    gfx_palette[COLOR_PANEL_DARK] =
        gfx_RGBTo1555(22, 30, 43);

    gfx_palette[COLOR_ACCENT] =
        gfx_RGBTo1555(59, 130, 246);

    gfx_palette[COLOR_TEXT] =
        gfx_RGBTo1555(241, 245, 249);

    gfx_palette[COLOR_TEXT_MUTED] =
        gfx_RGBTo1555(148, 163, 184);

    gfx_palette[COLOR_SELECTED] =
        gfx_RGBTo1555(51, 65, 85);

    gfx_palette[COLOR_PIN] =
        gfx_RGBTo1555(96, 165, 250);

    /*
     * Zytue67 gets its own username color.
     */
    gfx_palette[COLOR_ZYTUE] =
        gfx_RGBTo1555(168, 85, 247);

    gfx_palette[COLOR_HEART] =
        gfx_RGBTo1555(244, 63, 94);

    gfx_palette[COLOR_LIKE] =
        gfx_RGBTo1555(59, 130, 246);

    gfx_palette[COLOR_FUNNY] =
        gfx_RGBTo1555(250, 204, 21);

    gfx_palette[COLOR_SAD] =
        gfx_RGBTo1555(96, 165, 250);

    gfx_palette[COLOR_SUCCESS] =
        gfx_RGBTo1555(34, 197, 94);

    gfx_palette[COLOR_ERROR] =
        gfx_RGBTo1555(239, 68, 68);
}
