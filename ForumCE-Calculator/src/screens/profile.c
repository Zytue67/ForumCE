#include <tice.h>
#include <graphx.h>
#include <stdio.h>
#include <string.h>

#include "profile.h"
#include "messages.h"
#include "../network/network.h"
#include "../ui/theme.h"
#include "../ui/keyboard.h"

#define PEOPLE_PER_PAGE 10

static void draw_avatar(int x, int y, int r, int selected)
{
    uint8_t color = selected ? COLOR_ACCENT : COLOR_TEXT_MUTED;
    int headRadius = r / 4;
    int bodyRadius = r / 2;
    int headY, bodyY;

    if (headRadius < 1) headRadius = 1;
    if (bodyRadius < 2) bodyRadius = 2;
    headY = y - (r * 5) / 16;
    bodyY = y + (r * 5) / 16;
    if (bodyY + bodyRadius > y + r - 1) bodyY = y + r - bodyRadius - 1;

    gfx_SetColor(color);
    gfx_Circle(x, y, r);
    gfx_FillCircle(x, headY, headRadius);
    gfx_Circle(x, bodyY, bodyRadius);
}

static void print_centered(const char *text, int centerX, int y)
{
    gfx_PrintStringXY(text, centerX - gfx_GetStringWidth(text) / 2, y);
}

static void print_number_centered(unsigned int value, int x, int y)
{
    char buffer[16];
    sprintf(buffer, "%u", value);
    print_centered(buffer, x, y);
}

static void draw_wrapped_text(const char *text, int x, int y, int maxWidth, int maxLines)
{
    char line[64], word[32], test[64];
    int ll = 0, wl = 0, lc = 0, i = 0;
    line[0] = word[0] = 0;

    while (1)
    {
        char c = text ? text[i] : 0;
        if (c == ' ' || c == 0)
        {
            if (wl)
            {
                if (ll) snprintf(test, sizeof test, "%s %s", line, word);
                else snprintf(test, sizeof test, "%s", word);

                if (ll && gfx_GetStringWidth(test) > (unsigned int)maxWidth)
                {
                    gfx_PrintStringXY(line, x, y + lc * 11);
                    if (++lc >= maxLines) return;
                    strcpy(line, word);
                    ll = strlen(line);
                }
                else
                {
                    strcpy(line, test);
                    ll = strlen(line);
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

    if (ll && lc < maxLines) gfx_PrintStringXY(line, x, y + lc * 11);
}

static int is_self_name(const char *name)
{
    const ForumCEUser *me = network_user();
    return me && name && strcmp(me->username, name) == 0;
}

static void draw_profile_controls(const ForumCESocialUser *user, int selected)
{
    int isSelf = is_self_name(user->username);

    /* Redraw only the interactive stats/buttons region.  The header, avatar,
     * username and bio remain untouched while the cursor moves. */
    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(154, 50, 158, 79);

    if (selected == 0)
    {
        gfx_SetColor(COLOR_SELECTED);
        gfx_FillRectangle(158, 53, 68, 39);
    }
    else if (selected == 1)
    {
        gfx_SetColor(COLOR_SELECTED);
        gfx_FillRectangle(236, 53, 74, 39);
    }

    gfx_SetTextFGColor(selected == 0 ? COLOR_SELECTED_TEXT : COLOR_TEXT);
    print_number_centered(user->followers, 192, 58);
    gfx_SetTextFGColor(selected == 0 ? COLOR_SELECTED_TEXT : COLOR_TEXT_MUTED);
    print_centered("Followers", 192, 75);

    gfx_SetTextFGColor(selected == 1 ? COLOR_SELECTED_TEXT : COLOR_TEXT);
    print_number_centered(user->following, 273, 58);
    gfx_SetTextFGColor(selected == 1 ? COLOR_SELECTED_TEXT : COLOR_TEXT_MUTED);
    print_centered("Following", 273, 75);

    if (!isSelf)
    {
        gfx_SetColor(selected == 2 ? COLOR_SELECTED : COLOR_PANEL_DARK);
        gfx_FillRectangle(78, 103, 94, 23);
        gfx_SetTextFGColor(selected == 2 ? COLOR_SELECTED_TEXT : COLOR_TEXT);
        print_centered("Message", 125, 110);

        gfx_SetColor(selected == 3 ? COLOR_SELECTED : COLOR_PANEL_DARK);
        gfx_FillRectangle(180, 103, 126, 23);
        gfx_SetTextFGColor(selected == 3 ? COLOR_SELECTED_TEXT : COLOR_TEXT);
        print_centered(user->is_following ? "Following +" : "Follow", 243, 110);
    }
}

static void draw_profile(const ForumCESocialUser *user, int selected)
{
    char handle[26];
    int isSelf = is_self_name(user->username);

    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY("PROFILE", 12, 10);
    gfx_SetTextScale(1, 1); gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);

    draw_avatar(47, 79, 22, 0);
    gfx_SetTextFGColor(strcmp(user->username, "Zytue67") == 0 ? COLOR_ZYTUE : COLOR_TEXT);
    gfx_PrintStringXY(user->username, 78, 61);
    snprintf(handle, sizeof handle, "@%s", user->username);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(handle, 78, 77);

    draw_profile_controls(user, selected);

    gfx_SetTextFGColor(COLOR_TEXT_MUTED); gfx_PrintStringXY("BIO", 16, 142);
    gfx_SetColor(COLOR_PANEL_DARK); gfx_FillRectangle(14, 156, 292, 48);
    gfx_SetTextFGColor(COLOR_TEXT); draw_wrapped_text(user->bio, 23, 167, 272, 3);

    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    if (isSelf) gfx_PrintStringXY("<> Select  ENTER Open  CLEAR Back", 39, 226);
    else gfx_PrintStringXY("<> Select  ^v Row  ENTER Open  CLEAR", 19, 226);
}

static void draw_people_row(int row, int selected)
{
    const ForumCESocialUser *user = network_social_user(row);
    int y = 49 + row * 16;
    if (!user) return;

    gfx_SetColor(selected ? COLOR_SELECTED : COLOR_BACKGROUND);
    gfx_FillRectangle(10, y - 1, 300, 16);
    draw_avatar(24, y + 6, 6, selected);
    gfx_SetTextFGColor(strcmp(user->username, "Zytue67") == 0 ? COLOR_ZYTUE : COLOR_TEXT);
    gfx_PrintStringXY(user->username, 40, y + 2);
}

static void draw_people_list(const char *title, int page, int pages, int selected)
{
    int i;
    int count = network_social_user_count();
    char pageText[20];

    gfx_FillScreen(COLOR_BACKGROUND);
    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT); gfx_SetTextScale(2, 2); gfx_PrintStringXY(title, 12, 10);
    gfx_SetTextScale(1, 1);
    sprintf(pageText, "%d/%d", page, pages);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(pageText, 306 - gfx_GetStringWidth(pageText), 16);
    gfx_SetColor(COLOR_ACCENT); gfx_FillRectangle(0, 42, 320, 2);

    if (count == 0)
    {
        const char *empty = strcmp(title, "FOLLOWING") == 0 ? "No Following" :
                            strcmp(title, "FOLLOWERS") == 0 ? "No Followers" : "No People";
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        print_centered(empty, 160, 62);
    }

    for (i = 0; i < count && i < PEOPLE_PER_PAGE; i++) draw_people_row(i, i == selected);

    gfx_SetColor(COLOR_PANEL); gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    if (strcmp(title, "PEOPLE") == 0) gfx_PrintStringXY("^v Select  <> Page  ENTER Profile", 27, 226);
    else gfx_PrintStringXY("<> Switch  ^v Select  ENTER Profile", 27, 226);
}

static void follower_list(const char *profileName, int followingTab)
{
    int page = 1, selected = 0, dirty = 1;
    sk_key_t key;
    char name[FORUMCE_USERNAME_MAX];

    if (!network_load_relation(profileName, followingTab != 0, page)) return;

    while (1)
    {
        int count = network_social_user_count();
        int pages = network_social_pages();
        int oldSelected;
        char letter;

        if (selected >= count) selected = count ? count - 1 : 0;
        if (dirty)
        {
            draw_people_list(followingTab ? "FOLLOWING" : "FOLLOWERS", page, pages, selected);
            dirty = 0;
        }

        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) return;
        oldSelected = selected;

        if (key == sk_Left || key == sk_Right)
        {
            followingTab = !followingTab;
            page = 1;
            selected = 0;
            if (!network_load_relation(profileName, followingTab != 0, page)) return;
            dirty = 1;
            continue;
        }
        if (key == sk_Up)
        {
            if (selected > 0) selected--;
            else if (page > 1)
            {
                page--;
                if (!network_load_relation(profileName, followingTab != 0, page)) return;
                selected = network_social_user_count() ? network_social_user_count() - 1 : 0;
                dirty = 1;
            }
        }
        else if (key == sk_Down)
        {
            if (selected + 1 < count) selected++;
            else if (page < pages)
            {
                page++;
                selected = 0;
                if (!network_load_relation(profileName, followingTab != 0, page)) return;
                dirty = 1;
            }
        }
        else if (key == sk_Enter && count > 0)
        {
            const ForumCESocialUser *user = network_social_user(selected);
            strncpy(name, user->username, sizeof name - 1);
            name[sizeof name - 1] = 0;
            profile_screen_name(name);
            if (!network_load_relation(profileName, followingTab != 0, page)) return;
            dirty = 1;
        }
        else
        {
            letter = keyboard_alpha_letter(key);
            if (letter >= 'A' && letter <= 'Z')
            {
                int jumpPage, jumpSelected;
                if (network_jump_relation(profileName, followingTab != 0, letter, &jumpPage, &jumpSelected))
                {
                    page = jumpPage;
                    selected = jumpSelected;
                    if (!network_load_relation(profileName, followingTab != 0, page)) return;
                    dirty = 1;
                }
            }
        }

        if (!dirty && selected != oldSelected && count > 0)
        {
            draw_people_row(oldSelected, 0);
            draw_people_row(selected, 1);
        }
    }
}

void profile_screen_name(const char *username)
{
    char profileName[FORUMCE_USERNAME_MAX];
    int selected = 0;
    int dirty = 1;
    sk_key_t key;

    if (!username) return;
    strncpy(profileName, username, sizeof profileName - 1);
    profileName[sizeof profileName - 1] = 0;
    if (!network_load_profile(profileName)) return;

    while (1)
    {
        const ForumCESocialUser *user = network_profile();
        int isSelf;
        int oldSelected;
        if (!user) return;
        isSelf = is_self_name(user->username);
        if (dirty) { draw_profile(user, selected); dirty = 0; }

        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) break;
        oldSelected = selected;

        if (key == sk_Left)
        {
            if (selected == 1) selected = 0;
            else if (!isSelf && selected == 3) selected = 2;
        }
        else if (key == sk_Right)
        {
            if (selected == 0) selected = 1;
            else if (!isSelf && selected == 2) selected = 3;
        }
        else if (!isSelf && key == sk_Down)
        {
            if (selected == 0) selected = 2;
            else if (selected == 1) selected = 3;
        }
        else if (!isSelf && key == sk_Up)
        {
            if (selected == 2) selected = 0;
            else if (selected == 3) selected = 1;
        }
        else if (key == sk_Enter)
        {
            if (selected == 0) follower_list(profileName, 0);
            else if (selected == 1) follower_list(profileName, 1);
            else if (!isSelf && selected == 2) message_user_screen_name(profileName);
            else if (!isSelf && selected == 3) (void)network_set_follow(profileName, !user->is_following);

            if (!network_usb_connected() || !network_server_online()) break;
            if (!network_load_profile(profileName)) break;
            dirty = 1;
            continue;
        }

        if (selected != oldSelected) draw_profile_controls(user, selected);
    }
}

void profile_screen(int userIndex)
{
    const ForumCESocialUser *user = network_social_user(userIndex);
    char name[FORUMCE_USERNAME_MAX];
    if (!user) return;
    strncpy(name, user->username, sizeof name - 1);
    name[sizeof name - 1] = 0;
    profile_screen_name(name);
}

void my_profile_screen(void)
{
    const ForumCEUser *me = network_user();
    if (me && me->username[0]) profile_screen_name(me->username);
}

void people_screen(void)
{
    int page = 1, selected = 0, dirty = 1;
    sk_key_t key;
    char name[FORUMCE_USERNAME_MAX];

    if (!network_load_people(page)) return;

    while (1)
    {
        int count = network_social_user_count();
        int pages = network_social_pages();
        int oldSelected;
        char letter;

        if (selected >= count) selected = count ? count - 1 : 0;
        if (dirty) { draw_people_list("PEOPLE", page, pages, selected); dirty = 0; }

        do { network_update(); key = os_GetCSC(); } while (key == 0 && network_usb_connected() && network_server_online());
        if (key == sk_Clear) break;
        oldSelected = selected;

        if (key == sk_Up && selected > 0) selected--;
        else if (key == sk_Down && selected + 1 < count) selected++;
        else if (key == sk_Left && page > 1)
        {
            page--; selected = 0;
            if (!network_load_people(page)) break;
            dirty = 1;
        }
        else if (key == sk_Right && page < pages)
        {
            page++; selected = 0;
            if (!network_load_people(page)) break;
            dirty = 1;
        }
        else if (key == sk_Enter && count > 0)
        {
            const ForumCESocialUser *user = network_social_user(selected);
            strncpy(name, user->username, sizeof name - 1);
            name[sizeof name - 1] = 0;
            profile_screen_name(name);
            if (!network_load_people(page)) break;
            dirty = 1;
        }
        else
        {
            letter = keyboard_alpha_letter(key);
            if (letter >= 'A' && letter <= 'Z')
            {
                int jumpPage, jumpSelected;
                if (network_jump_people(letter, &jumpPage, &jumpSelected))
                {
                    page = jumpPage;
                    selected = jumpSelected;
                    if (!network_load_people(page)) break;
                    dirty = 1;
                }
            }
        }

        if (!dirty && selected != oldSelected && count > 0)
        {
            draw_people_row(oldSelected, 0);
            draw_people_row(selected, 1);
        }
    }
}
