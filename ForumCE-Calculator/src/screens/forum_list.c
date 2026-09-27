#include <tice.h>
#include <graphx.h>
#include <ti/getcsc.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "forum_list.h"
#include "thread.h"
#include "../ui/theme.h"
#include "../ui/keyboard.h"
#include "../network/network.h"

#define ITEMS_PER_PAGE 10
#define MAX_SEARCH 32
#define MAX_ITEMS 64

static void swap_items(
    ForumItem *a,
    ForumItem *b
)
{
    ForumItem temp = *a;
    *a = *b;
    *b = temp;
}

static void sort_forums(
    ForumItem *items,
    int count
)
{
    int i;
    int j;

    for (i = 0; i < count - 1; i++)
    {
        for (j = i + 1; j < count; j++)
        {
            if (items[j].pinned > items[i].pinned)
            {
                swap_items(
                    &items[i],
                    &items[j]
                );
            }
            else if (
                items[j].pinned == items[i].pinned &&
                items[j].modified > items[i].modified
            )
            {
                swap_items(
                    &items[i],
                    &items[j]
                );
            }
        }
    }
}

static char lower_char(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c + ('a' - 'A'));

    return c;
}

static int contains_case_insensitive(
    const char *text,
    const char *query
)
{
    int i;
    int j;

    if (query[0] == '\0')
        return 1;

    for (i = 0; text[i] != '\0'; i++)
    {
        for (j = 0; query[j] != '\0'; j++)
        {
            if (text[i + j] == '\0')
                break;

            if (
                lower_char(text[i + j]) !=
                lower_char(query[j])
            )
            {
                break;
            }
        }

        if (query[j] == '\0')
            return 1;
    }

    return 0;
}

static void draw_header(
    const char *categoryName,
    const char *search
)
{
    gfx_SetColor(COLOR_PANEL);

    gfx_FillRectangle(
        0,
        0,
        320,
        25
    );

    gfx_SetTextFGColor(COLOR_TEXT);

    gfx_SetTextScale(2, 2);

    gfx_PrintStringXY(
        categoryName,
        8,
        3
    );

    gfx_SetTextScale(1, 1);

    /*
     * Search bar.
     */
    gfx_SetColor(COLOR_PANEL_DARK);

    gfx_FillRectangle(
        8,
        28,
        304,
        17
    );

    gfx_SetColor(COLOR_ACCENT);

    gfx_Rectangle(
        8,
        28,
        304,
        17
    );

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    if (search[0] == '\0')
    {
        gfx_PrintStringXY(
            "Y= Search forums",
            14,
            33
        );
    }
    else
    {
        gfx_SetTextFGColor(COLOR_TEXT);

        gfx_PrintStringXY(
            search,
            14,
            33
        );
    }
}

static void draw_page_number(
    int page,
    int totalPages
)
{
    char buffer[20];

    sprintf(
        buffer,
        "%d/%d",
        page + 1,
        totalPages
    );

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    gfx_PrintStringXY(
        buffer,
        286,
        9
    );
}

static void draw_row(
    const ForumItem *item,
    int row,
    int selected
)
{
    int y;
    char title[38];

    y = 48 + row * 17;

    if (selected)
        gfx_SetColor(COLOR_SELECTED);
    else
        gfx_SetColor(COLOR_PANEL);

    gfx_FillRectangle(
        6,
        y,
        308,
        16
    );

    /*
     * Copy/truncate title so the date has its own area.
     */
    title[0] = '\0';

    {
        int i;
        int pos = 0;

        for (i = 0; item->title[i] != '\0' && pos < 34; i++)
        {
            title[pos++] = item->title[i];
        }

        if (item->title[i] != '\0' && pos >= 3)
        {
            title[pos - 3] = '.';
            title[pos - 2] = '.';
            title[pos - 1] = '.';
        }

        title[pos] = '\0';
    }

    gfx_SetTextFGColor(
        selected ? COLOR_TEXT : COLOR_TEXT
    );

    if (item->pinned)
    {
        gfx_SetTextFGColor(COLOR_PIN);

        gfx_PrintStringXY(
            "*",
            10,
            y + 3
        );
    }

    gfx_SetTextFGColor(
        selected ? COLOR_TEXT : COLOR_TEXT
    );

    gfx_PrintStringXY(
        title,
        20,
        y + 1
    );

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    gfx_PrintStringXY(
        item->author,
        20,
        y + 9
    );

    /*
     * Fixed metadata column.  Keep server date/time inside the
     * right-most 46 px so it can never run into the forum title.
     * lastResponse is stored as "MM/DD HH:MM".
     */
    {
        char metaDate[6] = "--/--";
        char metaTime[6] = "--:--";

        if (item->lastResponse && strlen(item->lastResponse) >= 11)
        {
            memcpy(metaDate, item->lastResponse, 5);
            metaDate[5] = '\0';
            memcpy(metaTime, item->lastResponse + 6, 5);
            metaTime[5] = '\0';
        }

        gfx_SetColor(COLOR_PANEL_DARK);
        gfx_FillRectangle(266, y + 1, 46, 14);
        gfx_SetTextFGColor(COLOR_ACCENT);
        gfx_PrintStringXY(metaDate, 270, y + 1);
        gfx_PrintStringXY(metaTime, 270, y + 8);
    }
}

static void draw_footer(void)
{
    gfx_SetColor(COLOR_PANEL_DARK);

    gfx_FillRectangle(
        0,
        220,
        320,
        20
    );

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    gfx_PrintStringXY(
        "^v Select",
        8,
        225
    );

    gfx_PrintStringXY(
        "ENTER Open",
        82,
        225
    );

    gfx_PrintStringXY(
        "Y= Search",
        160,
        225
    );
    gfx_PrintStringXY(
        "+ New",
        227,
        225
    );
    gfx_PrintStringXY(
        "CLEAR",
        276,
        225
    );
}

static void draw_list(
    const char *categoryName,
    const char *search,
    ForumItem *items,
    int *visible,
    int visibleCount,
    int page,
    int selected
)
{
    int i;
    int totalPages;
    int start;
    int row;

    totalPages =
        (visibleCount + ITEMS_PER_PAGE - 1)
        / ITEMS_PER_PAGE;

    if (totalPages < 1)
        totalPages = 1;

    start =
        page * ITEMS_PER_PAGE;

    gfx_SetDrawBuffer();

    gfx_FillScreen(
        COLOR_BACKGROUND
    );

    draw_header(
        categoryName,
        search
    );

    draw_page_number(
        page,
        totalPages
    );

    if (visibleCount == 0)
    {
        gfx_SetTextFGColor(
            COLOR_TEXT_MUTED
        );

        gfx_PrintStringXY(
            "No matching forums.",
            20,
            75
        );
    }
    else
    {
        for (i = 0; i < ITEMS_PER_PAGE; i++)
        {
            int index;

            index = start + i;

            if (index >= visibleCount)
                break;

            row = i;

            draw_row(
                &items[visible[index]],
                row,
                row == selected
            );
        }
    }

    draw_footer();

    gfx_BlitBuffer();

    gfx_SetDrawScreen();
}

#define SEARCH_TEXT_X          24
#define SEARCH_TEXT_RIGHT      296
#define SEARCH_TEXT_MAX_WIDTH  (SEARCH_TEXT_RIGHT - SEARCH_TEXT_X)

static void draw_search_input(
    const char *categoryName,
    const char *search,
    const KeyboardState *keyboard
)
{
    int cursorX;

    gfx_SetDrawBuffer();

    gfx_FillScreen(
        COLOR_BACKGROUND
    );

    /* Header */
    gfx_SetColor(COLOR_PANEL);

    gfx_FillRectangle(
        0,
        0,
        320,
        30
    );

    gfx_SetTextFGColor(COLOR_TEXT);

    gfx_SetTextScale(
        2,
        2
    );

    gfx_PrintStringXY(
        "SEARCH",
        8,
        4
    );

    gfx_SetTextScale(
        1,
        1
    );

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    gfx_PrintStringXY(
        categoryName,
        235,
        10
    );

    /* Search field */
    gfx_SetColor(COLOR_PANEL_DARK);

    gfx_FillRectangle(
        16,
        55,
        288,
        28
    );

    gfx_SetColor(COLOR_ACCENT);

    gfx_Rectangle(
        16,
        55,
        288,
        28
    );

    if (search[0] == '\0')
    {
        gfx_SetTextFGColor(
            COLOR_TEXT_MUTED
        );

        gfx_PrintStringXY(
            "Type to search...",
            24,
            63
        );
    }
    else
    {
        gfx_SetTextFGColor(
            COLOR_TEXT
        );

        gfx_PrintStringXY(
            search,
            SEARCH_TEXT_X,
            63
        );

        /*
         * gfx_VertLine's third argument is LENGTH,
         * not the ending Y coordinate.
         */
        cursorX =
            SEARCH_TEXT_X + gfx_GetStringWidth(search);

        if (cursorX < SEARCH_TEXT_RIGHT)
        {
            gfx_SetColor(COLOR_ACCENT);

            gfx_VertLine(
                cursorX,
                61,
                16
            );
        }
    }

    /* Help text */
    gfx_SetTextFGColor(
        COLOR_TEXT_MUTED
    );

    gfx_PrintStringXY(
        "ENTER  Search",
        18,
        103
    );

    gfx_PrintStringXY(
        "DEL  Backspace",
        18,
        121
    );

    gfx_PrintStringXY(
        "CLEAR  Cancel",
        18,
        139
    );

    /* Keyboard mode */
    gfx_SetTextFGColor(
        COLOR_ACCENT
    );

    gfx_PrintStringXY(
        "Input:",
        18,
        169
    );

    gfx_SetTextFGColor(
        COLOR_TEXT
    );

    gfx_PrintStringXY(
        keyboard_mode_name(keyboard),
        68,
        169
    );

    /* Bottom status bar */
    gfx_SetColor(COLOR_PANEL_DARK);

    gfx_FillRectangle(
        0,
        214,
        320,
        26
    );

    gfx_SetTextFGColor(
        COLOR_TEXT_MUTED
    );

    gfx_PrintStringXY(
        "Type a forum name and press ENTER",
        14,
        222
    );

    gfx_BlitBuffer();

    gfx_SetDrawScreen();
}

static void search_input(
    const char *categoryName,
    char *search
)
{
    KeyboardState keyboard;
    sk_key_t key;
    char ch;
    int length;

    keyboard_reset(&keyboard);

    /*
     * Search starts in alphabet mode so the user can immediately
     * type a forum name instead of having to press ALPHA first.
     */
    keyboard.mode = KEYBOARD_UPPER;

    draw_search_input(
        categoryName,
        search,
        &keyboard
    );

    while (1)
    {
        key = os_GetCSC();

        if (key == 0)
            continue;

        if (key == sk_Clear)
            return;

        if (key == sk_Enter)
            return;

        if (key == sk_Del)
        {
            length = strlen(search);

            if (length > 0)
                search[length - 1] = '\0';

            draw_search_input(
                categoryName,
                search,
                &keyboard
            );

            continue;
        }

        if (
            keyboard_handle_key(
                &keyboard,
                key,
                &ch
            )
        )
        {
            length = strlen(search);

            if (
                length < MAX_SEARCH - 1 &&
                ch != '\0'
            )
            {
                char nextChar[2];

                nextChar[0] = ch;
                nextChar[1] = '\0';

                /*
                 * Limit the search by PIXEL WIDTH, not character count.
                 * This keeps the text and cursor inside the search bar
                 * even when wide characters are entered.
                 */
                if (
                    gfx_GetStringWidth(search) +
                    gfx_GetStringWidth(nextChar) <=
                    SEARCH_TEXT_MAX_WIDTH
                )
                {
                    search[length] = ch;
                    search[length + 1] = '\0';
                }
            }

            draw_search_input(
                categoryName,
                search,
                &keyboard
            );
        }
        else
        {
            /*
             * Redraw for ALPHA/2nd-ALPHA mode changes so the
             * current keyboard mode is always visible.
             */
            draw_search_input(
                categoryName,
                search,
                &keyboard
            );
        }
    }
}

static int create_forum_input(void)
{
    char name[FORUMCE_FORUM_NAME_MAX];
    KeyboardState keyboard;
    sk_key_t key;
    char ch;
    int length = 0;
    name[0] = '\0';
    keyboard_reset(&keyboard);
    keyboard.mode = KEYBOARD_UPPER;

    while (1)
    {
        gfx_SetDrawBuffer();
        gfx_FillScreen(COLOR_BACKGROUND);
        gfx_SetColor(COLOR_PANEL);
        gfx_FillRectangle(0,0,320,30);
        gfx_SetTextFGColor(COLOR_TEXT);
        gfx_SetTextScale(2,2);
        gfx_PrintStringXY("NEW FORUM",8,4);
        gfx_SetTextScale(1,1);

        gfx_SetColor(COLOR_PANEL_DARK);
        gfx_FillRectangle(16,55,288,28);
        gfx_SetColor(COLOR_ACCENT);
        gfx_Rectangle(16,55,288,28);
        gfx_SetTextFGColor(name[0] ? COLOR_TEXT : COLOR_TEXT_MUTED);
        gfx_PrintStringXY(name[0] ? name : "Forum name...",24,63);

        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY("ENTER Create",18,105);
        gfx_PrintStringXY("DEL Backspace",18,123);
        gfx_PrintStringXY("CLEAR Cancel",18,141);
        gfx_SetTextFGColor(COLOR_ACCENT);
        gfx_PrintStringXY("Input:",18,170);
        gfx_SetTextFGColor(COLOR_TEXT);
        gfx_PrintStringXY(keyboard_mode_name(&keyboard),68,170);
        gfx_BlitBuffer();
        gfx_SetDrawScreen();

        network_update();
        if (!network_usb_connected() || !network_server_online()) return 0;
        key=os_GetCSC();
        if (key==sk_Clear) return 0;
        if (key==sk_Del) {
            if(length>0) name[--length]='\0';
            continue;
        }
        if (key==sk_Enter) {
            if(length==0) continue;
            return network_create_forum(name) ? 1 : 0;
        }
        if (keyboard_handle_key(&keyboard,key,&ch) && ch!='\0' && length<FORUMCE_FORUM_NAME_MAX-1) {
            name[length++]=ch; name[length]='\0';
        }
    }
}

static int confirm_delete_forum(const ForumItem *item)
{
    sk_key_t key;
    if (!item || item->pinned || item->forumId <= 0) return 0;

    gfx_SetDrawBuffer();
    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0,0,320,30);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2,2);
    gfx_PrintStringXY("DELETE FORUM",8,4);
    gfx_SetTextScale(1,1);

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("Delete this forum and all replies?",26,72);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_PrintStringXY(item->title,26,94);
    gfx_SetTextFGColor(COLOR_ACCENT);
    gfx_PrintStringXY("ENTER Delete",26,138);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("CLEAR Cancel",26,158);
    gfx_BlitBuffer();
    gfx_SetDrawScreen();

    while (1)
    {
        network_update();
        if (!network_usb_connected() || !network_server_online()) return 0;
        key = os_GetCSC();
        if (key == sk_Clear) return 0;
        if (key == sk_Enter) return network_delete_forum(item->forumId) ? 1 : 0;
    }
}

int forum_list_screen(
    const char *categoryName,
    ForumItem *items,
    int count
)
{
    int visible[MAX_ITEMS];
    int visibleCount;
    int page;
    int selected;
    int dirty = 1;
    char search[MAX_SEARCH];
    sk_key_t key;
    int i;

    if (count > MAX_ITEMS)
        count = MAX_ITEMS;

    sort_forums(items, count);
    search[0] = '\0';
    page = 0;
    selected = 0;

    while (1)
    {
        int totalPages;
        int start;
        int end;
        int oldSelected;

        network_update();
        if (!network_usb_connected() || !network_server_online()) return 0;

        visibleCount = 0;
        for (i = 0; i < count; i++)
        {
            if (contains_case_insensitive(items[i].title, search) ||
                contains_case_insensitive(items[i].author, search))
            {
                if (visibleCount < MAX_ITEMS)
                    visible[visibleCount++] = i;
            }
        }

        totalPages = (visibleCount + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
        if (totalPages < 1) totalPages = 1;
        if (page >= totalPages) page = totalPages - 1;

        start = page * ITEMS_PER_PAGE;
        end = start + ITEMS_PER_PAGE;
        if (end > visibleCount) end = visibleCount;
        if (selected >= end - start) selected = (end - start) - 1;
        if (selected < 0) selected = 0;

        if (dirty)
        {
            draw_list(categoryName, search, items, visible, visibleCount, page, selected);
            dirty = 0;
        }

        key = os_GetCSC();
        if (key == 0) continue;
        oldSelected = selected;

        if (key == sk_Up)
        {
            if (selected > 0) selected--;
        }
        else if (key == sk_Down)
        {
            if (selected < (end - start) - 1) selected++;
        }
        else if (key == sk_Left)
        {
            if (page > 0)
            {
                page--;
                selected = 0;
                dirty = 1;
            }
        }
        else if (key == sk_Right)
        {
            if (page < totalPages - 1)
            {
                page++;
                selected = 0;
                dirty = 1;
            }
        }
        else if (key == sk_Yequ)
        {
            search_input(categoryName, search);
            page = 0;
            selected = 0;
            dirty = 1;
        }
        else if (key == sk_Enter)
        {
            if (visibleCount > 0 && selected >= 0 && start + selected < visibleCount)
            {
                int itemIndex = visible[start + selected];
                thread_screen(items[itemIndex].title, items[itemIndex].threadId);
                return 1;
            }
        }
        else if (key == sk_Del)
        {
            if (visibleCount > 0 && selected >= 0 && start + selected < visibleCount)
            {
                int itemIndex = visible[start + selected];
                if (!items[itemIndex].pinned && confirm_delete_forum(&items[itemIndex]))
                    return 1;
                dirty = 1;
            }
        }
        else if (key == sk_Add)
        {
            if (create_forum_input()) return 1;
            dirty = 1;
        }
        else if (key == sk_Clear)
        {
            return 0;
        }

        /* Cursor movement is the common case.  Redraw only the two affected
         * rows instead of rebuilding and blitting the entire 320x240 screen. */
        if (!dirty && selected != oldSelected && visibleCount > 0)
        {
            if (start + oldSelected < visibleCount)
                draw_row(&items[visible[start + oldSelected]], oldSelected, 0);
            if (start + selected < visibleCount)
                draw_row(&items[visible[start + selected]], selected, 1);
        }
    }
}
