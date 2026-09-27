#include <tice.h>
#include <graphx.h>
#include <stdio.h>
#include <string.h>

#include "messages.h"
#include "profile.h"
#include "../network/network.h"
#include "../ui/theme.h"
#include "../ui/keyboard.h"

#define CONVS_PER_PAGE 6
#define SEARCH_MAX 24
#define DM_MAX 160
#define CHAT_LEFT_X 14
#define CHAT_RIGHT_X 306
#define BUBBLE_MAX_WIDTH 205
#define BUBBLE_TEXT_WIDTH 189
#define CHAT_TOP 52
#define CHAT_BOTTOM 215
#define CHAR_HEART 1
#define CHAR_THUMBS 2
#define CHAR_FUNNY 3
#define CHAR_SAD 4
#define INLINE_CHAR_WIDTH 10

static char composeText[DM_MAX + 1];

static void draw_avatar(int x, int y, int r, int selected)
{
    uint8_t color = selected ? COLOR_ACCENT : COLOR_TEXT_MUTED;
    gfx_SetColor(color);
    gfx_Circle(x, y, r);
    gfx_FillCircle(x, y - 2, r / 4);
    gfx_Circle(x, y + 3, r / 2);
}

static int is_special_char(char c)
{
    return c == CHAR_HEART || c == CHAR_THUMBS || c == CHAR_FUNNY || c == CHAR_SAD;
}

static int inline_char_width(char c)
{
    return is_special_char(c) ? INLINE_CHAR_WIDTH : gfx_GetCharWidth(c);
}

static int inline_string_width(const char *text)
{
    int width = 0;
    int i;
    for (i = 0; text && text[i]; i++) width += inline_char_width(text[i]);
    return width;
}

static void draw_heart(int x, int y)
{
    gfx_SetColor(COLOR_HEART);
    gfx_FillRectangle(x + 1, y, 2, 2); gfx_FillRectangle(x + 5, y, 2, 2);
    gfx_FillRectangle(x, y + 2, 8, 3); gfx_FillRectangle(x + 1, y + 5, 6, 2);
    gfx_FillRectangle(x + 3, y + 7, 2, 1);
}

static void draw_thumbs(int x, int y)
{
    gfx_SetColor(COLOR_LIKE);
    gfx_FillRectangle(x + 2, y + 3, 6, 5); gfx_FillRectangle(x + 5, y + 1, 2, 4);
    gfx_FillRectangle(x + 6, y, 2, 3); gfx_FillRectangle(x, y + 4, 2, 4);
}

static void draw_funny(int x, int y)
{
    gfx_SetColor(COLOR_FUNNY);
    gfx_FillRectangle(x + 1, y, 6, 1); gfx_FillRectangle(x, y + 1, 8, 6);
    gfx_FillRectangle(x + 1, y + 7, 6, 1);
    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(x + 2, y + 2, 1, 1); gfx_FillRectangle(x + 5, y + 2, 1, 1);
    gfx_FillRectangle(x + 2, y + 5, 4, 1);
}

static void draw_sad(int x, int y)
{
    gfx_SetColor(COLOR_SAD);
    gfx_FillRectangle(x + 1, y, 6, 1); gfx_FillRectangle(x, y + 1, 8, 6);
    gfx_FillRectangle(x + 1, y + 7, 6, 1);
    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(x + 2, y + 2, 1, 1); gfx_FillRectangle(x + 5, y + 2, 1, 1);
    gfx_FillRectangle(x + 2, y + 6, 4, 1);
}

static void draw_special_character(char c, int x, int y)
{
    if (c == CHAR_HEART) draw_heart(x, y);
    else if (c == CHAR_THUMBS) draw_thumbs(x, y);
    else if (c == CHAR_FUNNY) draw_funny(x, y);
    else if (c == CHAR_SAD) draw_sad(x, y);
}

static void draw_inline_string(const char *text, int x, int y)
{
    int i;
    int cx = x;
    for (i = 0; text && text[i]; i++)
    {
        if (is_special_char(text[i]))
        {
            draw_special_character(text[i], cx, y);
            cx += INLINE_CHAR_WIDTH;
        }
        else
        {
            char b[2] = {text[i], 0};
            gfx_PrintStringXY(b, cx, y);
            cx += gfx_GetCharWidth(text[i]);
        }
    }
}

static void draw_character_menu(void)
{
    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(72, 70, 176, 86);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_PrintStringXY("1 Heart", 96, 84); gfx_PrintStringXY("2 Like", 96, 101);
    gfx_PrintStringXY("3 Funny", 96, 118); gfx_PrintStringXY("4 Sad", 96, 135);
    draw_heart(82, 84); draw_thumbs(82, 101); draw_funny(82, 118); draw_sad(82, 135);
}

static int character_menu(char *message, int *length)
{
    sk_key_t key;
    draw_character_menu();
    while (1)
    {
        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected());
        if (key == sk_Clear) return 0;
        if (key == sk_1 || key == sk_2 || key == sk_3 || key == sk_4)
        {
            char c = key == sk_1 ? CHAR_HEART : key == sk_2 ? CHAR_THUMBS : key == sk_3 ? CHAR_FUNNY : CHAR_SAD;
            if (*length < DM_MAX)
            {
                message[*length] = c;
                (*length)++;
                message[*length] = 0;
            }
            return 1;
        }
    }
}

static int wrap_text(const char *text, char lines[][48], int maxLines, int maxWidth)
{
    char word[32], current[48], test[48];
    int wl = 0, cl = 0, lc = 0, i = 0;
    word[0] = current[0] = 0;
    while (1)
    {
        char c = text ? text[i] : 0;
        if (c == ' ' || c == 0)
        {
            if (wl)
            {
                if (cl) snprintf(test, sizeof test, "%s %s", current, word);
                else snprintf(test, sizeof test, "%s", word);
                if (cl && inline_string_width(test) > maxWidth)
                {
                    if (lc < maxLines) strcpy(lines[lc++], current);
                    strcpy(current, word);
                    cl = strlen(current);
                }
                else
                {
                    strcpy(current, test);
                    cl = strlen(current);
                }
                wl = 0;
                word[0] = 0;
            }
            if (c == 0) break;
        }
        else if (wl < (int)sizeof(word) - 1)
        {
            word[wl++] = c;
            word[wl] = 0;
        }
        i++;
    }
    if (cl && lc < maxLines) strcpy(lines[lc++], current);
    return lc;
}

static int bubble_height(const char *text)
{
    char lines[8][48];
    int n = wrap_text(text, lines, 8, BUBBLE_TEXT_WIDTH);
    if (n < 1) n = 1;
    return 13 + n * 10 + 15;
}

static void stamp_parts(const char *stamp, char *date, char *tm)
{
    if (stamp && strlen(stamp) >= 16)
    {
        date[0] = stamp[5]; date[1] = stamp[6]; date[2] = '/'; date[3] = stamp[8]; date[4] = stamp[9]; date[5] = 0;
        tm[0] = stamp[11]; tm[1] = stamp[12]; tm[2] = ':'; tm[3] = stamp[14]; tm[4] = stamp[15]; tm[5] = 0;
    }
    else
    {
        strcpy(date, "--/--");
        strcpy(tm, "--:--");
    }
}

static int draw_bubble(const char *text, int mine, int y, const char *date, const char *tm)
{
    char lines[8][48];
    int n = wrap_text(text, lines, 8, BUBBLE_TEXT_WIDTH);
    int i, width = 74, height, x;
    if (n < 1) { n = 1; lines[0][0] = 0; }
    for (i = 0; i < n; i++)
    {
        int w = inline_string_width(lines[i]) + 16;
        if (w > width) width = w;
    }
    {
        int mw = gfx_GetStringWidth(date) + gfx_GetStringWidth(tm) + 25;
        if (mw > width) width = mw;
    }
    if (width > BUBBLE_MAX_WIDTH) width = BUBBLE_MAX_WIDTH;
    height = 13 + n * 10 + 15;
    x = mine ? CHAT_RIGHT_X - width : CHAT_LEFT_X;

    gfx_SetColor(mine ? COLOR_ACCENT : COLOR_PANEL_DARK);
    gfx_FillRectangle(x, y, width, height);
    gfx_SetTextFGColor(COLOR_TEXT);
    for (i = 0; i < n; i++) draw_inline_string(lines[i], x + 7, y + 7 + i * 10);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(date, x + 7, y + height - 11);
    gfx_PrintStringXY(tm, x + width - 7 - gfx_GetStringWidth(tm), y + height - 11);
    return height;
}

static void draw_chat_header(const char *username)
{
    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 44);
    draw_avatar(23, 22, 10, 0);
    gfx_SetTextFGColor(strcmp(username, "Zytue67") == 0 ? COLOR_ZYTUE : COLOR_TEXT);
    gfx_SetTextScale(1, 1);
    gfx_PrintStringXY(username, 42, 12);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("ENTER Profile", 42, 27);
    gfx_SetColor(COLOR_ACCENT);
    gfx_FillRectangle(0, 42, 320, 2);
}

static void draw_chat_footer(void)
{
    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("^ Older  v Newer  + Message  CLEAR Back", 13, 226);
}

static void draw_conversation(const char *username)
{
    int count = network_dm_count();
    int i;
    int y = CHAT_TOP;

    gfx_FillScreen(COLOR_BACKGROUND);
    draw_chat_header(username);

    if (network_dm_total() == 0)
    {
        const char *empty = "No messages yet";
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY(empty, 160 - gfx_GetStringWidth(empty) / 2, 78);
        draw_chat_footer();
        return;
    }

    /* Each bubble already contains its own date and time, so the old
     * stand-alone conversation date row was redundant and pushed the first
     * message down. Start the current three-message window directly below
     * the header. */
    for (i = 0; i < count; i++)
    {
        const ForumCEDirectMessage *m = network_dm(i);
        char date[6], tm[6];
        int h;
        if (!m) continue;
        h = bubble_height(m->body);
        if (i > 0 && y + 7 + h <= CHAT_BOTTOM) y += 7;
        else if (i > 0 && y + h > CHAT_BOTTOM) break;
        if (y + h > CHAT_BOTTOM) break;
        stamp_parts(m->created_at, date, tm);
        y += draw_bubble(m->body, m->mine, y, date, tm);
    }

    draw_chat_footer();
}

static void draw_sync(const char *title, const char *line)
{
    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY(title, 12, 10);
    gfx_SetTextScale(1, 1); gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED); gfx_PrintStringXY(line, 160 - gfx_GetStringWidth(line) / 2, 112);
}

static void draw_compose_static(const char *username)
{
    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY("MESSAGE", 12, 10);
    gfx_SetTextScale(1, 1); gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(username, 306 - gfx_GetStringWidth(username), 17);
    gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("DEL Back  ENTER Send  + Chars  CLEAR Cancel", 15, 226);
}

static void draw_compose_editor(const KeyboardState *kb, int len)
{
    char count[24], preview[48];
    int previewStart = 0;

    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(12, 56, 296, 151);
    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(14, 60, 292, 116);

    if (!len)
    {
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY("Write a message...", 24, 72);
    }
    else
    {
        int i;
        for (i = 0; i < len; i++)
        {
            if (inline_string_width(&composeText[i]) < 250) { previewStart = i; break; }
        }
        strncpy(preview, &composeText[previewStart], sizeof preview - 1);
        preview[sizeof preview - 1] = 0;
        gfx_SetTextFGColor(COLOR_TEXT);
        draw_inline_string(preview, 24, 72);
        {
            int cx = 24 + inline_string_width(preview);
            if (cx > 294) cx = 294;
            gfx_SetColor(COLOR_ACCENT);
            gfx_VertLine(cx, 70, 12);
        }
    }

    sprintf(count, "%d/%d", len, DM_MAX);
    gfx_SetTextFGColor(COLOR_ACCENT);
    gfx_PrintStringXY(keyboard_mode_name(kb), 20, 187);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(count, 306 - gfx_GetStringWidth(count), 187);
}

static int compose_message(const char *username)
{
    char out = 0;
    int len = 0;
    KeyboardState kb;
    sk_key_t key;

    composeText[0] = 0;
    keyboard_reset(&kb);
    kb.mode = KEYBOARD_UPPER;
    draw_compose_static(username);
    draw_compose_editor(&kb, len);

    while (1)
    {
        KeyboardMode oldMode;
        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) return 0;
        if (key == sk_Del)
        {
            if (len > 0) composeText[--len] = 0;
            draw_compose_editor(&kb, len);
            continue;
        }
        if (key == sk_Add)
        {
            (void)character_menu(composeText, &len);
            draw_compose_static(username);
            draw_compose_editor(&kb, len);
            continue;
        }
        if (key == sk_Enter)
        {
            if (!len) return 0;
            draw_sync("MESSAGE", "Sending to ForumCE...");
            if (network_send_dm(username, composeText)) return 1;
            draw_sync("SEND FAILED", "ENTER Retry   CLEAR Cancel");
            while (1)
            {
                do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected());
                if (key == sk_Clear) return 0;
                if (key == sk_Enter) break;
            }
            draw_compose_static(username);
            draw_compose_editor(&kb, len);
            continue;
        }

        oldMode = kb.mode;
        if (keyboard_handle_key(&kb, key, &out) && out && len < DM_MAX)
        {
            composeText[len++] = out;
            composeText[len] = 0;
            draw_compose_editor(&kb, len);
        }
        else if (kb.mode != oldMode)
        {
            draw_compose_editor(&kb, len);
        }
    }
}

static void conversation_screen(const char *username)
{
    int offset = 0;
    int dirty = 1;
    sk_key_t key;

    if (!network_load_dm_window(username, offset)) return;
    if (network_dm_total() > 0)
    {
        network_mark_dm_read(username);
        network_request_unread();
    }

    while (1)
    {
        int total = network_dm_total();
        int maxOffset = total > FORUMCE_DM_WINDOW ? total - FORUMCE_DM_WINDOW : 0;

        if (offset > maxOffset) offset = maxOffset;
        if (dirty) { draw_conversation(username); dirty = 0; }
        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) break;

        if (key == sk_Up && offset < maxOffset)
        {
            offset++;
            if (!network_load_dm_window(username, offset)) break;
            dirty = 1;
        }
        else if (key == sk_Down && offset > 0)
        {
            offset--;
            if (!network_load_dm_window(username, offset)) break;
            dirty = 1;
        }
        else if (key == sk_Add)
        {
            if (compose_message(username))
            {
                offset = 0;
                if (!network_load_dm_window(username, offset)) break;
                network_mark_dm_read(username);
                network_request_unread();
            }
            dirty = 1;
        }
        else if (key == sk_Enter)
        {
            profile_screen_name(username);
            if (!network_load_dm_window(username, offset)) break;
            dirty = 1;
        }
    }
}

void message_user_screen_name(const char *username)
{
    if (username && username[0]) conversation_screen(username);
}

void message_user_screen(int userIndex)
{
    const ForumCESocialUser *u = network_social_user(userIndex);
    char name[FORUMCE_USERNAME_MAX];
    if (!u) return;
    strncpy(name, u->username, sizeof name - 1);
    name[sizeof name - 1] = 0;
    conversation_screen(name);
}

static int confirm_delete(const char *username)
{
    int stage;
    sk_key_t key;
    char line[48];
    for (stage = 0; stage < 2; stage++)
    {
        gfx_FillScreen(COLOR_BACKGROUND);
        gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
        gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY("DELETE DM", 12, 10);
        gfx_SetTextScale(1, 1); gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);
        snprintf(line, sizeof line, stage ? "Delete conversation with %s?" : "Hide conversation with %s?", username);
        gfx_SetTextFGColor(COLOR_TEXT); gfx_PrintStringXY(line, 160 - gfx_GetStringWidth(line) / 2, 94);
        gfx_SetTextFGColor(COLOR_TEXT_MUTED); gfx_PrintStringXY("ENTER Confirm", 56, 137); gfx_PrintStringXY("CLEAR Cancel", 186, 137);
        while (1)
        {
            do { network_update(); key = os_GetCSC(); } while (key == 0);
            if (key == sk_Clear) return 0;
            if (key == sk_Enter) break;
        }
    }
    return 1;
}

static void draw_search_row(int row, int selected)
{
    const ForumCESocialUser *u = network_social_user(row);
    int y = 96 + row * 16;
    if (!u) return;
    gfx_SetColor(selected ? COLOR_SELECTED : COLOR_BACKGROUND);
    gfx_FillRectangle(14, y - 3, 292, 16);
    gfx_SetTextFGColor(strcmp(u->username, "Zytue67") == 0 ? COLOR_ZYTUE : COLOR_TEXT);
    gfx_PrintStringXY(u->username, 24, y);
}

static void draw_search_screen(const char *query, int len, int selected)
{
    int i;
    int count = network_social_user_count();
    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY("SEARCH", 12, 10);
    gfx_SetTextScale(1, 1); gfx_SetTextFGColor(COLOR_TEXT_MUTED); gfx_PrintStringXY("USERS", 270, 17);
    gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);
    gfx_SetColor(COLOR_PANEL_DARK); gfx_FillRectangle(14, 54, 292, 30);
    gfx_SetTextFGColor(len ? COLOR_TEXT : COLOR_TEXT_MUTED);
    gfx_PrintStringXY(len ? query : "Search usernames...", 23, 65);
    for (i = 0; i < count && i < 7; i++) draw_search_row(i, i == selected);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED); gfx_PrintStringXY("^v Select  ENTER Open  DEL Erase  CLEAR Back", 18, 226);
}

static int search_user(char *result, size_t resultSize)
{
    char query[SEARCH_MAX + 1], out = 0;
    int len = 0, selected = 0;
    KeyboardState kb;
    sk_key_t key;

    query[0] = 0;
    keyboard_reset(&kb);
    kb.mode = KEYBOARD_UPPER;
    network_search_users("");
    draw_search_screen(query, len, selected);

    while (1)
    {
        int count = network_social_user_count();
        int oldSelected = selected;
        if (selected >= count) selected = count ? count - 1 : 0;
        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) return 0;

        if (key == sk_Up && selected > 0) selected--;
        else if (key == sk_Down && selected < count - 1) selected++;
        else if (key == sk_Del && len > 0)
        {
            query[--len] = 0;
            selected = 0;
            network_search_users(query);
            draw_search_screen(query, len, selected);
            continue;
        }
        else if (key == sk_Enter && count > 0)
        {
            const ForumCESocialUser *u = network_social_user(selected);
            strncpy(result, u->username, resultSize - 1);
            result[resultSize - 1] = 0;
            return 1;
        }
        else if (keyboard_handle_key(&kb, key, &out) && out && len < SEARCH_MAX)
        {
            if (gfx_GetStringWidth(query) + gfx_GetCharWidth(out) < 260)
            {
                query[len++] = out;
                query[len] = 0;
                selected = 0;
                network_search_users(query);
                draw_search_screen(query, len, selected);
                continue;
            }
        }

        if (selected != oldSelected && count > 0)
        {
            draw_search_row(oldSelected, 0);
            draw_search_row(selected, 1);
        }
    }
}

static void draw_message_list_row(int row, int selected)
{
    const ForumCEConversation *cv = network_conversation(row);
    char buffer[56];
    int y = 82 + row * 22;
    if (!cv) return;

    gfx_SetColor(selected ? COLOR_SELECTED : COLOR_BACKGROUND);
    gfx_FillRectangle(8, y - 3, 304, 21);
    draw_avatar(24, y + 7, 7, selected);
    gfx_SetTextFGColor(strcmp(cv->username, "Zytue67") == 0 ? COLOR_ZYTUE : COLOR_TEXT);
    gfx_PrintStringXY(cv->username, 40, y);

    if (cv->unread > 0)
    {
        sprintf(buffer, "(%u new messages)", cv->unread);
        gfx_SetTextFGColor(COLOR_ACCENT);
        gfx_PrintStringXY(buffer, 40, y + 10);
        gfx_SetColor(COLOR_ACCENT);
        gfx_FillCircle(302, y + 5, 3);
    }
    else
    {
        if (cv->last_mine) snprintf(buffer, sizeof buffer, "You: %.19s", cv->preview);
        else { strncpy(buffer, cv->preview, 24); buffer[24] = 0; }
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY(buffer, 40, y + 10);
    }
}

static void draw_messages_list(int page, int selected)
{
    int i;
    int count = network_conversation_count();
    int pages = network_conversation_pages();
    char buffer[56];

    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY("MESSAGES", 12, 10);
    gfx_SetTextScale(1, 1);
    if (network_unread_messages() > 0)
    {
        sprintf(buffer, "%u new", network_unread_messages());
        gfx_SetTextFGColor(COLOR_ACCENT);
        gfx_PrintStringXY(buffer, 306 - gfx_GetStringWidth(buffer), 16);
    }
    gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);
    gfx_SetColor(COLOR_PANEL_DARK); gfx_FillRectangle(12, 50, 296, 24);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED); gfx_PrintStringXY("Y= Search users", 22, 58);

    if (count == 0)
    {
        const char *empty = "No Messages";
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY(empty, 160 - gfx_GetStringWidth(empty) / 2, 86);
    }
    for (i = 0; i < count && i < CONVS_PER_PAGE; i++) draw_message_list_row(i, i == selected);

    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 220, 320, 20);
    sprintf(buffer, "%d/%d", page, pages);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(buffer, 6, 226);
    gfx_PrintStringXY("^v Select <> Page ENTER Open DEL Delete", 43, 226);
}

void messages_screen(void)
{
    int page = 1, selected = 0, dirty = 1;
    sk_key_t key;
    char username[FORUMCE_USERNAME_MAX];

    if (!network_load_conversations(page)) return;

    while (1)
    {
        int count = network_conversation_count();
        int pages = network_conversation_pages();
        int oldSelected;

        if (page > pages)
        {
            page = pages > 0 ? pages : 1;
            selected = 0;
            if (!network_load_conversations(page)) break;
            dirty = 1;
            continue;
        }
        if (selected >= count) selected = count ? count - 1 : 0;
        if (dirty) { draw_messages_list(page, selected); dirty = 0; }

        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) break;
        oldSelected = selected;

        if (key == sk_Up && selected > 0) selected--;
        else if (key == sk_Down && selected + 1 < count) selected++;
        else if (key == sk_Left && page > 1)
        {
            page--; selected = 0;
            if (!network_load_conversations(page)) break;
            dirty = 1;
        }
        else if (key == sk_Right && page < pages)
        {
            page++; selected = 0;
            if (!network_load_conversations(page)) break;
            dirty = 1;
        }
        else if (key == sk_Yequ)
        {
            if (search_user(username, sizeof username)) conversation_screen(username);
            if (!network_load_conversations(page)) break;
            dirty = 1;
        }
        else if (key == sk_Enter && count > 0)
        {
            const ForumCEConversation *cv = network_conversation(selected);
            strncpy(username, cv->username, sizeof username - 1);
            username[sizeof username - 1] = 0;
            conversation_screen(username);
            if (!network_load_conversations(page)) break;
            dirty = 1;
        }
        else if (key == sk_Del && count > 0)
        {
            const ForumCEConversation *cv = network_conversation(selected);
            strncpy(username, cv->username, sizeof username - 1);
            username[sizeof username - 1] = 0;
            if (confirm_delete(username))
            {
                draw_sync("DELETE DM", "Syncing with ForumCE...");
                network_delete_dm(username);
                selected = 0;
                if (!network_load_conversations(page)) break;
            }
            dirty = 1;
        }

        if (!dirty && selected != oldSelected && count > 0)
        {
            draw_message_list_row(oldSelected, 0);
            draw_message_list_row(selected, 1);
        }
    }

    network_request_unread();
}
