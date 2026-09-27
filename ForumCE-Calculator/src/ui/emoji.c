#include <graphx.h>

#include "emoji.h"
#include "theme.h"

void emoji_draw(
    int type,
    int x,
    int y,
    int size
)
{
    if (type == EMOJI_HEART)
    {
        gfx_SetColor(COLOR_HEART);

        gfx_FillCircle(
            x - size / 2,
            y - size / 3,
            size / 2
        );

        gfx_FillCircle(
            x + size / 2,
            y - size / 3,
            size / 2
        );

        gfx_FillTriangle(
            x - size,
            y,
            x + size,
            y,
            x,
            y + size + 2
        );
    }

    else if (type == EMOJI_LIKE)
    {
        gfx_SetColor(COLOR_LIKE);

        gfx_FillRectangle(
            x - 3,
            y - 2,
            7,
            size + 5
        );

        gfx_FillRectangle(
            x - 7,
            y + 1,
            4,
            size + 2
        );

        gfx_FillRectangle(
            x + 3,
            y - size / 2,
            5,
            size + 2
        );
    }

    else if (type == EMOJI_FUNNY)
    {
        gfx_SetColor(COLOR_FUNNY);

        gfx_FillCircle(
            x,
            y,
            size
        );

        gfx_SetColor(COLOR_BACKGROUND);

        gfx_FillCircle(
            x - 3,
            y - 2,
            1
        );

        gfx_FillCircle(
            x + 3,
            y - 2,
            1
        );

        gfx_Line(
            x - 4,
            y + 3,
            x,
            y + 5
        );

        gfx_Line(
            x,
            y + 5,
            x + 4,
            y + 3
        );
    }

    else if (type == EMOJI_SAD)
    {
        gfx_SetColor(COLOR_SAD);

        gfx_FillCircle(
            x,
            y,
            size
        );

        gfx_SetColor(COLOR_BACKGROUND);

        gfx_FillCircle(
            x - 3,
            y - 2,
            1
        );

        gfx_FillCircle(
            x + 3,
            y - 2,
            1
        );

        gfx_Line(
            x - 4,
            y + 5,
            x,
            y + 3
        );

        gfx_Line(
            x,
            y + 3,
            x + 4,
            y + 5
        );
    }
}
