#include <tice.h>
#include <graphx.h>
#include <stdio.h>
#include <string.h>

#include "notifications.h"
#include "../network/network.h"
#include "../ui/theme.h"

#define ROW_Y 55
#define ROW_H 16
#define ROW_W 304

static void stamp_short(const char *stamp, char *out, size_t size)
{
    if (stamp && strlen(stamp) >= 16)
        snprintf(out, size, "%c%c/%c%c %c%c:%c%c", stamp[5],stamp[6],stamp[8],stamp[9],stamp[11],stamp[12],stamp[14],stamp[15]);
    else
        snprintf(out, size, "--/-- --:--");
}

static void clipped_text(const char *source, char *out, size_t size, int maxWidth)
{
    size_t len;
    if (!source) source = "";
    strncpy(out, source, size - 1);
    out[size - 1] = 0;
    if (gfx_GetStringWidth(out) <= (unsigned int)maxWidth) return;
    len = strlen(out);
    while (len > 3 && gfx_GetStringWidth(out) > (unsigned int)maxWidth)
        out[--len] = 0;
    if (len >= 3)
    {
        out[len - 3] = '.';
        out[len - 2] = '.';
        out[len - 1] = '.';
    }
}

static void draw_header(int page, int pages)
{
    char p[20];
    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0,0,320,44);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2,2);
    gfx_PrintStringXY("ALERTS",12,10);
    gfx_SetTextScale(1,1);
    sprintf(p,"%d/%d",page,pages);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(p,306-gfx_GetStringWidth(p),17);
    gfx_SetColor(COLOR_ACCENT);
    gfx_FillRectangle(0,42,320,2);
}

static void draw_row(int row)
{
    const ForumCENotification *n = network_notification(row);
    char stamp[16], text[FORUMCE_NOTIFICATION_TEXT_MAX];
    int y = ROW_Y + row * ROW_H;
    if (!n) return;

    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(8,y-2,ROW_W,ROW_H);

    if (n->unread)
    {
        gfx_SetColor(COLOR_ACCENT);
        gfx_FillCircle(15,y+5,2);
    }

    stamp_short(n->created_at,stamp,sizeof stamp);
    clipped_text(n->text,text,sizeof text,210);

    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_PrintStringXY(text,22,y+1);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(stamp,306-gfx_GetStringWidth(stamp),y+1);
}

static void draw_page(int page)
{
    int i, count = network_notification_count();
    draw_header(page, network_notification_pages());

    if (count == 0)
    {
        const char *empty = "No Notifications";
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY(empty,160-gfx_GetStringWidth(empty)/2,72);
    }
    else
    {
        for (i=0;i<count;i++) draw_row(i);
    }

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0,220,320,20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("<> Page                 CLEAR Back",39,226);
}

void notification_feed_screen(void)
{
    int page = 1;
    sk_key_t key;

    if (!network_load_notifications(page)) return;
    draw_page(page);

    /* Marking read happens only after the list is safely on screen. */
    (void)network_mark_notifications_read();

    while (1)
    {
        int pages = network_notification_pages();
        do { network_update(); key=os_GetCSC(); }
        while (key==0 && network_usb_connected() && network_server_online());

        if (key == sk_Clear) return;
        if (key == sk_Left && page > 1)
        {
            page--;
            if (!network_load_notifications(page)) return;
            draw_page(page);
        }
        else if (key == sk_Right && page < pages)
        {
            page++;
            if (!network_load_notifications(page)) return;
            draw_page(page);
        }
    }
}
