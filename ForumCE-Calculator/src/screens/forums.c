#include <tice.h>
#include <graphx.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "forums.h"
#include "forum_list.h"
#include "../network/network.h"
#include "../ui/theme.h"

#define MAX_SERVER_FORUMS FORUMCE_MAX_FORUMS

static ForumItem server_items[MAX_SERVER_FORUMS];
static char server_titles[MAX_SERVER_FORUMS][FORUMCE_FORUM_NAME_MAX];
static char server_authors[MAX_SERVER_FORUMS][FORUMCE_AUTHOR_MAX];
static char server_dates[MAX_SERVER_FORUMS][12];

static void draw_loading(void)
{
    gfx_SetDrawBuffer();
    gfx_FillScreen(COLOR_BACKGROUND);

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 25);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("FORUMS", 8, 3);
    gfx_SetTextScale(1, 1);

    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(8, 28, 304, 17);
    gfx_SetColor(COLOR_ACCENT);
    gfx_Rectangle(8, 28, 304, 17);

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("Loading forums from server...", 66, 104);

    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("CLEAR Back", 245, 225);

    gfx_BlitBuffer();
    gfx_SetDrawScreen();
}

static void draw_error(const char *message)
{
    gfx_SetDrawBuffer();
    gfx_FillScreen(COLOR_BACKGROUND);

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 25);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("FORUMS", 8, 3);
    gfx_SetTextScale(1, 1);

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(message, 20, 75);

    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("CLEAR Back", 245, 225);

    gfx_BlitBuffer();
    gfx_SetDrawScreen();
}

void forums_screen(void)
{
    int i;
    int count;

    while (1)
    {
        network_request_forums();
        draw_loading();

        while (!network_forums_ready())
        {
            network_update();

            if (os_GetCSC() == sk_Clear)
                return;

            if (!network_usb_connected() || !network_server_online())
            {
                draw_error("ForumCE connection lost.");
                while (os_GetCSC() != sk_Clear)
                    network_update();
                return;
            }
        }

        count = network_forum_count();

        if (count <= 0)
        {
            draw_error("No forums available.");
            while (os_GetCSC() != sk_Clear)
                network_update();
            return;
        }

        if (count > MAX_SERVER_FORUMS)
            count = MAX_SERVER_FORUMS;

        for (i = 0; i < count; i++)
        {
            const ForumCEForum *forum = network_forum(i);

            if (forum)
            {
                strncpy(server_titles[i], forum->name, FORUMCE_FORUM_NAME_MAX - 1);
                server_titles[i][FORUMCE_FORUM_NAME_MAX - 1] = '\0';

                strncpy(server_authors[i], forum->author, FORUMCE_AUTHOR_MAX - 1);
                server_authors[i][FORUMCE_AUTHOR_MAX - 1] = '\0';
                if (forum->last_activity[0] && strlen(forum->last_activity) >= 16) {
                    /* Server format YYYY-MM-DD HH:MM[:SS[.fff]] -> MM/DD HH:MM. */
                    snprintf(server_dates[i], sizeof server_dates[i], "%c%c/%c%c %c%c:%c%c",
                        forum->last_activity[5], forum->last_activity[6], forum->last_activity[8], forum->last_activity[9],
                        forum->last_activity[11], forum->last_activity[12], forum->last_activity[14], forum->last_activity[15]);
                } else strcpy(server_dates[i], "--/-- --:--");

                server_items[i].title = server_titles[i];
                server_items[i].author = server_authors[i];
                server_items[i].lastResponse = server_dates[i];
                /* The server already returns pinned first, then newest activity. */
                server_items[i].modified = 100000L - i;
                server_items[i].pinned = forum->id == 1;
                server_items[i].forumId = forum->id;
                server_items[i].threadId = forum->thread_id;
            }
        }

        /* 1 = reload after returning from a thread/create/delete, 0 = Back. */
        if (!forum_list_screen("FORUMS", server_items, count))
            return;
    }
}
