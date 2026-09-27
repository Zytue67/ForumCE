#include <tice.h>
#include <graphx.h>
#include <stdio.h>

#include "home.h"
#include "forums.h"
#include "messages.h"
#include "profile.h"
#include "settings.h"
#include "../ui/theme.h"
#include "../network/network.h"

static const char *menuItems[] = {
    "Forums",
    "Messages",
    "People",
    "My Profile",
    "Settings"
};

#define MENU_COUNT 5
#define HOME_ROW_X 12
#define HOME_ROW_W 296
#define HOME_ROW_H 24

static int home_row_y(int row)
{
    return 55 + row * 30;
}

static void draw_home_clock(void)
{
    gfx_SetTextScale(1, 1);
    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(238, 4, 76, 32);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    if (network_clock_ready())
    {
        const char *dateText = network_clock_date();
        const char *timeText = network_clock_time();
        gfx_PrintStringXY(dateText, 276 - gfx_GetStringWidth(dateText) / 2, 9);
        gfx_PrintStringXY(timeText, 276 - gfx_GetStringWidth(timeText) / 2, 22);
    }
    else
    {
        gfx_PrintStringXY("--/--", 256, 9);
        gfx_PrintStringXY("--:--", 256, 22);
    }
}

static void draw_home_row(int row, int selected)
{
    int y = home_row_y(row);
    char unread[12];

    gfx_SetColor(selected ? COLOR_SELECTED : COLOR_BACKGROUND);
    gfx_FillRectangle(HOME_ROW_X, y - 3, HOME_ROW_W, HOME_ROW_H);
    gfx_SetTextFGColor(selected ? COLOR_SELECTED_TEXT : COLOR_TEXT);
    gfx_PrintStringXY(selected ? "> " : "  ", 20, y + 2);
    gfx_PrintStringXY(menuItems[row], 38, y + 2);

    if (row == 1 && network_unread_messages() > 0)
    {
        unsigned int total = network_unread_messages();
        sprintf(unread, "%u", total);
        gfx_SetColor(COLOR_ACCENT);
        gfx_FillCircle(290, y + 7, 8);
        gfx_SetTextFGColor(COLOR_TEXT);
        gfx_PrintStringXY(unread, 290 - gfx_GetStringWidth(unread) / 2, y + 3);
    }
}

static void draw_home(int selected)
{
    int i;

    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 42);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("FORUMCE", 12, 10);

    draw_home_clock();

    gfx_SetColor(COLOR_ACCENT);
    gfx_FillRectangle(0, 41, 320, 2);
    gfx_SetTextScale(1, 1);

    for (i = 0; i < MENU_COUNT; i++)
        draw_home_row(i, i == selected);

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("^v Select     ENTER Open", 70, 226);
}

void home_screen(void)
{
    int selected = 0;
    sk_key_t key;
    unsigned int seenClock;
    unsigned int seenUnread;

    network_request_clock();
    network_request_unread();
    draw_home(selected);
    seenClock = network_clock_version();
    seenUnread = network_unread_version();

    while (1)
    {
        int oldSelected;

        network_update();
        network_maybe_request_clock();

        if (!network_usb_connected() || !network_server_online() || !network_authenticated())
            break;

        if (network_clock_version() != seenClock)
        {
            seenClock = network_clock_version();
            draw_home_clock();
        }

        if (network_unread_version() != seenUnread)
        {
            seenUnread = network_unread_version();
            draw_home_row(1, selected == 1);
        }

        key = os_GetCSC();
        oldSelected = selected;

        if (key == sk_Up && selected > 0)
            selected--;
        else if (key == sk_Down && selected < MENU_COUNT - 1)
            selected++;
        else if (key == sk_Enter)
        {
            if (selected == 0) forums_screen();
            else if (selected == 1) messages_screen();
            else if (selected == 2) people_screen();
            else if (selected == 3) my_profile_screen();
            else settings_screen();

            if (!network_usb_connected() || !network_server_online() || !network_authenticated())
                break;

            network_request_unread();
            draw_home(selected);
            seenClock = network_clock_version();
            seenUnread = network_unread_version();
            continue;
        }
        else if (key == sk_Clear)
        {
            break;
        }

        if (selected != oldSelected)
        {
            draw_home_row(oldSelected, 0);
            draw_home_row(selected, 1);
        }
    }
}
