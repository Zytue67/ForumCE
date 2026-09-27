#include <tice.h>
#include <graphx.h>
#include <ti/getcsc.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "thread.h"
#include "../ui/theme.h"
#include "../ui/keyboard.h"
#include "../network/network.h"

#define ROOTS_PER_PAGE       10
#define MAX_REPLY_CACHE      30
#define REPLIES_PER_PAGE     10
#define MAX_AUTHOR            20
#define MAX_MESSAGE          320

#define CONTENT_TOP           65
#define CONTENT_BOTTOM       216

#define ACTION_VIEW            0
#define ACTION_REPLY           1
#define ACTION_LIKE            2
#define ACTION_CANCEL          3

#define CHAR_HEART             1
#define CHAR_THUMBS            2
#define CHAR_FUNNY             3
#define CHAR_SAD               4

#define INLINE_CHAR_WIDTH     10
#define THREAD_LOAD_TIMEOUT_SECONDS 8

/*
 * IMPORTANT MEMORY DESIGN
 * ------------------------
 * The calculator never stores 500 full posts.
 *
 * Only the current 10 root posts are loaded into rootCache[].
 * Only up to 30 replies for the currently opened root are loaded
 * into replyCache[].  The server can later replace these loaders
 * with API requests for page N.
 */
typedef struct
{
    char author[MAX_AUTHOR];
    char message[MAX_MESSAGE];
    char time[9];
    char date[12];
    char replyTo[MAX_AUTHOR];
    int id;
    int replyRootId;
    long likes;
    bool liked;
    int replyCount;
} Post;

static Post rootCache[ROOTS_PER_PAGE];
static Post replyCache[MAX_REPLY_CACHE];

static int totalRootPosts;
static int currentRootPage;
static int rootCacheCount;
static int replyCount;
static int activeThreadId = 0;

/* Keep the 320-byte composer buffer out of the nested function stack. */
static char composerMessage[MAX_MESSAGE];


static void copy_string(char *dst, const char *src, int maxLength)
{
    strncpy(dst, src, maxLength - 1);
    dst[maxLength - 1] = '\0';
}

static void format_like_count(long value, char *buffer)
{
    if (value < 1000)
        sprintf(buffer, "%ld", value);
    else if (value < 10000)
    {
        long whole = value / 1000;
        long decimal = (value % 1000) / 100;
        if (decimal == 0) sprintf(buffer, "%ldk", whole);
        else sprintf(buffer, "%ld.%ldk", whole, decimal);
    }
    else if (value < 1000000)
        sprintf(buffer, "%ldk", value / 1000);
    else
    {
        long whole = value / 1000000;
        long decimal = (value % 1000000) / 100000;
        if (decimal == 0) sprintf(buffer, "%ldM", whole);
        else sprintf(buffer, "%ld.%ldM", whole, decimal);
    }
}

static void draw_like_icon(int x, int y)
{
    gfx_SetColor(COLOR_TEXT_MUTED);
    gfx_FillRectangle(x, y + 4, 8, 7);
    gfx_FillRectangle(x + 4, y + 1, 4, 5);
    gfx_FillRectangle(x + 6, y, 3, 5);
    gfx_FillRectangle(x + 8, y + 5, 5, 2);
    gfx_FillRectangle(x + 8, y + 7, 5, 2);
    gfx_FillRectangle(x + 8, y + 9, 4, 2);
}

static int special_char_width(char c)
{
    unsigned char uc = (unsigned char)c;

    if (c == CHAR_HEART || c == CHAR_THUMBS ||
        c == CHAR_FUNNY || c == CHAR_SAD)
        return INLINE_CHAR_WIDTH;

    if (uc < 32 || uc > 126)
        return gfx_GetCharWidth('?');

    return gfx_GetCharWidth(c);
}

static void draw_heart(int x, int y)
{
    gfx_SetColor(COLOR_HEART);
    gfx_FillRectangle(x + 1, y, 2, 2);
    gfx_FillRectangle(x + 5, y, 2, 2);
    gfx_FillRectangle(x, y + 2, 8, 3);
    gfx_FillRectangle(x + 1, y + 5, 6, 2);
    gfx_FillRectangle(x + 3, y + 7, 2, 1);
}

static void draw_thumbs(int x, int y)
{
    gfx_SetColor(COLOR_LIKE);
    gfx_FillRectangle(x + 2, y + 3, 6, 5);
    gfx_FillRectangle(x + 5, y + 1, 2, 4);
    gfx_FillRectangle(x + 6, y, 2, 3);
    gfx_FillRectangle(x, y + 4, 2, 4);
}

static void draw_funny(int x, int y)
{
    gfx_SetColor(COLOR_FUNNY);
    gfx_FillRectangle(x + 1, y, 6, 1);
    gfx_FillRectangle(x, y + 1, 8, 6);
    gfx_FillRectangle(x + 1, y + 7, 6, 1);
    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(x + 2, y + 2, 1, 1);
    gfx_FillRectangle(x + 5, y + 2, 1, 1);
    gfx_FillRectangle(x + 2, y + 5, 4, 1);
}

static void draw_sad(int x, int y)
{
    gfx_SetColor(COLOR_SAD);
    gfx_FillRectangle(x + 1, y, 6, 1);
    gfx_FillRectangle(x, y + 1, 8, 6);
    gfx_FillRectangle(x + 1, y + 7, 6, 1);
    gfx_SetColor(COLOR_BACKGROUND);
    gfx_FillRectangle(x + 2, y + 2, 1, 1);
    gfx_FillRectangle(x + 5, y + 2, 1, 1);
    gfx_FillRectangle(x + 2, y + 5, 1, 1);
    gfx_FillRectangle(x + 3, y + 4, 2, 1);
    gfx_FillRectangle(x + 5, y + 5, 1, 1);
}

static void draw_special_character(char c, int x, int y)
{
    if (c == CHAR_HEART) draw_heart(x, y);
    else if (c == CHAR_THUMBS) draw_thumbs(x, y);
    else if (c == CHAR_FUNNY) draw_funny(x, y);
    else if (c == CHAR_SAD) draw_sad(x, y);
}

static int get_line_count(const char *text, int maxWidth)
{
    int lines = 1;
    int width = 0;
    int i;

    for (i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        int w;

        if (c == '\n')
        {
            lines++;
            width = 0;
            continue;
        }

        w = special_char_width(c);
        if (width + w > maxWidth)
        {
            lines++;
            width = w;
        }
        else
            width += w;
    }

    return lines;
}

static void draw_wrapped_text(const char *text, int x, int y, int maxWidth)
{
    int currentX = x;
    int currentY = y;
    int i;

    for (i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        int w;

        if (c == '\n')
        {
            currentX = x;
            currentY += 9;
            continue;
        }

        w = special_char_width(c);
        if (currentX + w > x + maxWidth)
        {
            currentX = x;
            currentY += 9;
        }

        if (c == CHAR_HEART || c == CHAR_THUMBS ||
            c == CHAR_FUNNY || c == CHAR_SAD)
            draw_special_character(c, currentX, currentY);
        else
        {
            unsigned char uc = (unsigned char)c;
            gfx_SetTextXY(currentX, currentY);
            gfx_PrintChar((uc < 32 || uc > 126) ? '?' : c);
        }

        currentX += w;
    }
}

static int reply_count_for_root(int rootId);

static int post_height(const Post *post, bool showReplyCount)
{
    int lines = get_line_count(post->message, 278);
    int extra = 0;

    if (post->replyTo[0] != '\0') extra += 12;
    if (showReplyCount && post->replyRootId == -1)
    {
        if (reply_count_for_root(post->id) > 0)
            extra += 12;
    }

    return 36 + extra + lines * 9 + 16;
}

static void draw_reply_target(const Post *post, int y)
{
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("> Reply to:", 15, y);

    if (strcmp(post->replyTo, "Zytue67") == 0)
        gfx_SetTextFGColor(COLOR_ZYTUE);
    else
        gfx_SetTextFGColor(COLOR_ACCENT);

    gfx_PrintStringXY(post->replyTo, 88, y);
}

static void draw_post(const Post *post, int y, bool selected, bool showReplyCount)
{
    int height = post_height(post, showReplyCount);
    int messageY;
    int lines;
    int likeY;
    char likeText[16];
    int replyCount = 0;
    char replyText[24];

    if (selected) gfx_SetColor(COLOR_SELECTED);
    else gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(7, y, 306, height - 3);

    if (selected)
    {
        gfx_SetColor(COLOR_ACCENT);
        gfx_Rectangle(7, y, 306, height - 3);
    }

    if (strcmp(post->author, "Zytue67") == 0)
        gfx_SetTextFGColor(COLOR_ZYTUE);
    else
        gfx_SetTextFGColor(COLOR_ACCENT);
    gfx_PrintStringXY(post->author, 15, y + 3);

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(post->date, 205, y + 3);
    gfx_PrintStringXY(post->time, 267, y + 3);

    if (post->replyTo[0] != '\0')
    {
        draw_reply_target(post, y + 15);
        messageY = y + 27;
    }
    else
        messageY = y + 15;

    gfx_SetTextFGColor(COLOR_TEXT);
    lines = get_line_count(post->message, 278);
    draw_wrapped_text(post->message, 15, messageY, 278);

    likeY = messageY + lines * 9 + 9;

    if (showReplyCount && post->replyRootId == -1)
        replyCount = reply_count_for_root(post->id);

    if (replyCount > 0)
    {
        sprintf(replyText, "View %d %s", replyCount,
                replyCount == 1 ? "Reply" : "Replies");
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY(replyText, 15, likeY);
        likeY += 11;
    }

    format_like_count(post->likes, likeText);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(likeText, 267, likeY);
    draw_like_icon(290, likeY);
}

static int root_page_count(void)
{
    int pages = (totalRootPosts + ROOTS_PER_PAGE - 1) / ROOTS_PER_PAGE;
    return pages < 1 ? 1 : pages;
}

static void clear_post(Post *post)
{
    memset(post, 0, sizeof(Post));
    post->replyRootId = -1;
}

static void build_root_post(int pageIndex, Post *post)
{
    const ForumCEPost *src;
    clear_post(post);
    src = network_post(pageIndex);
    if (!src) return;
    post->id = src->id;
    post->replyRootId = -1;
    copy_string(post->author, src->author, MAX_AUTHOR);
    copy_string(post->message, src->body, MAX_MESSAGE);
    post->likes = src->likes;
    post->liked = src->liked;
    if (src->created_at[0]) {
        char d[12] = "";
        char t[9] = "";
        if (strlen(src->created_at) >= 16) {
            sprintf(d, "%c%c/%c%c", src->created_at[5],src->created_at[6],src->created_at[8],src->created_at[9]);
            sprintf(t, "%c%c:%c%c", src->created_at[11],src->created_at[12],src->created_at[14],src->created_at[15]);
        }
        copy_string(post->date,d,12); copy_string(post->time,t,9);
    }
    post->replyCount = src->reply_count;
}

static bool load_root_page(int page)
{
    int i;
    int count;
    clock_t started;

    if (page < 0) page = 0;
    if (activeThreadId <= 0) return false;

    network_request_posts(activeThreadId, page + 1);
    started = clock();

    while (network_posts_loading())
    {
        network_update();

        if (!network_usb_connected() || !network_server_online())
            return false;

        if ((clock() - started) >=
            (clock_t)(THREAD_LOAD_TIMEOUT_SECONDS * CLOCKS_PER_SEC))
            return false;
    }

    if (!network_posts_ready())
        return false;

    count = network_post_count();
    if (count < 0) count = 0;
    if (count > ROOTS_PER_PAGE) count = ROOTS_PER_PAGE;

    totalRootPosts = network_post_total();
    if (totalRootPosts < 0) totalRootPosts = 0;
    currentRootPage = page;
    rootCacheCount = count;

    for (i = 0; i < ROOTS_PER_PAGE; i++)
    {
        clear_post(&rootCache[i]);
        if (i < rootCacheCount)
            build_root_post(i, &rootCache[i]);
    }

    return true;
}

static bool initialize_thread(int threadId)
{
    memset(rootCache, 0, sizeof(rootCache));
    memset(replyCache, 0, sizeof(replyCache));

    totalRootPosts = 0;
    currentRootPage = 0;
    rootCacheCount = 0;
    replyCount = 0;
    activeThreadId = threadId;

    /* rootCache is rebuilt by load_root_page(); no 500-entry metadata table
     * is needed.  Keeping reply counts with the ten visible roots saves RAM
     * and also works with arbitrary server post IDs. */
    return load_root_page(0);
}

static Post *find_root_post(int rootId)
{
    int i;
    for (i = 0; i < rootCacheCount; i++)
    {
        if (rootCache[i].id == rootId)
            return &rootCache[i];
    }
    return NULL;
}

static int reply_count_for_root(int rootId)
{
    Post *root = find_root_post(rootId);
    return root ? root->replyCount : 0;
}

static int reply_index_at_position(int rootId, int position)
{
    int count = 0;
    int i;

    for (i = 0; i < replyCount; i++)
    {
        if (replyCache[i].replyRootId != rootId)
            continue;

        if (count == position)
            return i;

        count++;
    }

    return -1;
}

static int reply_page_count(int rootId)
{
    int count = reply_count_for_root(rootId);
    if (count <= 0) return 1;
    return (count + REPLIES_PER_PAGE - 1) / REPLIES_PER_PAGE;
}

static void load_replies_for_root(int rootId)
{
    int page;
    int total = 0;
    replyCount = 0;

    for (page = 1; page <= 3 && replyCount < MAX_REPLY_CACHE; page++)
    {
        int i;
        clock_t started;
        network_request_replies(rootId, page);
        started = clock();
        while (network_replies_loading())
        {
            network_update();
            if (!network_usb_connected() || !network_server_online()) return;
            if ((clock() - started) >=
                (clock_t)(THREAD_LOAD_TIMEOUT_SECONDS * CLOCKS_PER_SEC))
                return;
        }
        if (!network_replies_ready()) return;
        total = network_reply_total();

        for (i = 0; i < network_reply_count() && replyCount < MAX_REPLY_CACHE; i++)
        {
            const ForumCEReply *src = network_reply(i);
            Post *dst;
            if (!src) continue;
            dst = &replyCache[replyCount++];
            clear_post(dst);
            dst->id = src->id;
            dst->replyRootId = rootId;
            copy_string(dst->author, src->author, MAX_AUTHOR);
            copy_string(dst->message, src->body, MAX_MESSAGE);
            copy_string(dst->replyTo, src->author, MAX_AUTHOR);
            dst->likes = src->likes;
            dst->liked = src->liked;
            if (src->created_at[0] && strlen(src->created_at) >= 16)
            {
                char d[12] = "";
                char t[9] = "";
                sprintf(d, "%c%c/%c%c", src->created_at[5],src->created_at[6],src->created_at[8],src->created_at[9]);
                sprintf(t, "%c%c:%c%c", src->created_at[11],src->created_at[12],src->created_at[14],src->created_at[15]);
                copy_string(dst->date,d,12); copy_string(dst->time,t,9);
            }
        }
        if (replyCount >= total) break;
    }
}

static void draw_header(const char *title)
{
    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 41);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY(title, 9, 4);
    gfx_SetTextScale(1, 1);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_SetColor(COLOR_ACCENT);
    gfx_FillRectangle(0, 41, 320, 2);
}

static void adjust_root_offset(int selected, int *offset)
{
    int y;
    int i;

    if (rootCacheCount <= 0)
    {
        *offset = 0;
        return;
    }

    if (selected < 0) return;
    if (*offset < 0) *offset = 0;
    if (*offset >= rootCacheCount) *offset = rootCacheCount - 1;

    if (selected < *offset)
    {
        *offset = selected;
        return;
    }

    while (*offset < selected)
    {
        y = CONTENT_TOP;
        for (i = *offset; i <= selected; i++)
        {
            int h = post_height(&rootCache[i], true);
            if (y + h > CONTENT_BOTTOM)
                break;
            y += h + 4;
        }

        if (y + post_height(&rootCache[selected], true) <= CONTENT_BOTTOM)
            break;

        (*offset)++;
    }
}

static void draw_thread(const char *forumTitle, int selected, int offset, bool present)
{
    int i;
    int y = CONTENT_TOP;
    char pageText[24];

    gfx_SetDrawBuffer();
    gfx_FillScreen(COLOR_BACKGROUND);
    draw_header("THREAD");

    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 1);
    gfx_PrintStringXY(forumTitle, 10, 47);
    gfx_SetTextScale(1, 1);

    if (rootCacheCount <= 0)
    {
        gfx_SetTextFGColor(COLOR_TEXT_MUTED);
        gfx_PrintStringXY("No posts yet.", 15, CONTENT_TOP + 8);
    }
    else
    {
        for (i = 0; i < rootCacheCount; i++)
        {
            int height;
            if (i < offset) continue;
            if (y >= CONTENT_BOTTOM) break;
            height = post_height(&rootCache[i], true);
            draw_post(&rootCache[i], y, i == selected, true);
            y += height + 4;
        }
    }

    /* Two-line footer: same controls, fixed zones, no collisions. */
    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(0, 218, 320, 22);
    sprintf(pageText, "Pg %d/%d", currentRootPage + 1, root_page_count());
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("^v Select", 5, 220);
    gfx_PrintStringXY("<> Page", 76, 220);
    gfx_PrintStringXY(pageText, 135, 220);
    gfx_PrintStringXY("ENTER Options", 190, 220);
    gfx_PrintStringXY("+ Post", 5, 231);
    gfx_PrintStringXY("CLEAR Back", 244, 231);

    if (present)
    {
        gfx_BlitBuffer();
        gfx_SetDrawScreen();
    }
}

static void draw_action_menu(int option, int replyCount)
{
    gfx_SetDrawBuffer();
    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(52, 52, 216, 145);
    gfx_SetColor(COLOR_ACCENT);
    gfx_Rectangle(52, 52, 216, 145);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("POST", 125, 60);
    gfx_SetTextScale(1, 1);

    gfx_SetColor(COLOR_SELECTED);
    gfx_FillRectangle(70, 91 + option * 22, 180, 18);
    gfx_SetTextFGColor(COLOR_TEXT);

    if (replyCount > 0) gfx_PrintStringXY("1  View Replies", 82, 94);
    else gfx_PrintStringXY("1  View Replies", 82, 94);
    gfx_PrintStringXY("2  Reply", 82, 116);
    gfx_PrintStringXY("3  Like / Unlike", 82, 138);
    gfx_PrintStringXY("4  Cancel", 82, 160);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("ENTER Select", 105, 181);
}

static int action_menu(const char *forumTitle, const Post *post, int selected, int offset)
{
    sk_key_t key;
    int option = 0;
    int replies = reply_count_for_root(post->id);

    while (1)
    {
        draw_thread(forumTitle, selected, offset, false);
        draw_action_menu(option, replies);
        gfx_BlitBuffer();
        gfx_SetDrawScreen();

        key = os_GetCSC();
        if (key == sk_Up)
        {
            if (option > 0) option--;
            else option = ACTION_CANCEL;
        }
        else if (key == sk_Down)
        {
            if (option < ACTION_CANCEL) option++;
            else option = 0;
        }
        else if (key == sk_Enter)
        {
            return option;
        }
        else if (key == sk_Clear)
        {
            return ACTION_CANCEL;
        }
    }
}

static int reply_position_for_index(int rootId, int cacheIndex)
{
    int position = 0;
    int i;

    for (i = 0; i < replyCount; i++)
    {
        if (replyCache[i].replyRootId != rootId)
            continue;
        if (i == cacheIndex)
            return position;
        position++;
    }

    return -1;
}

static int reply_page_first_position(int page)
{
    if (page < 0) page = 0;
    return page * REPLIES_PER_PAGE;
}

static int reply_view_item_height(
    int rootId,
    int page,
    int virtualIndex,
    int pageReplyCount
)
{
    if (virtualIndex == 0)
        return post_height(find_root_post(rootId), false);

    if (virtualIndex - 1 < pageReplyCount)
    {
        int position = reply_page_first_position(page) + virtualIndex - 1;
        int cacheIndex = reply_index_at_position(rootId, position);
        if (cacheIndex >= 0)
            return post_height(&replyCache[cacheIndex], false);
    }

    return 0;
}

static int reply_virtual_item_for_selected(
    int page,
    int selected,
    int pageReplyCount
)
{
    int first = reply_page_first_position(page);
    if (selected < 0) return 0;
    if (selected < first) return 0;
    if (selected >= first + pageReplyCount) return pageReplyCount;
    return selected - first + 1;
}

static int reply_page_item_count(int rootId, int page)
{
    int total = reply_count_for_root(rootId);
    int first = reply_page_first_position(page);
    int left = total - first;
    if (left < 0) left = 0;
    if (left > REPLIES_PER_PAGE) left = REPLIES_PER_PAGE;
    return left;
}

static void adjust_reply_offset(
    int rootId,
    int page,
    int selected,
    int pageReplyCount,
    int *offset
)
{
    int itemCount = pageReplyCount + 1;
    int target = reply_virtual_item_for_selected(page, selected, pageReplyCount);
    int y;
    int i;

    if (*offset < 0) *offset = 0;
    if (*offset >= itemCount) *offset = itemCount - 1;

    if (target < *offset)
    {
        *offset = target;
        return;
    }

    while (*offset < target)
    {
        y = CONTENT_TOP;
        for (i = *offset; i <= target; i++)
        {
            int h = reply_view_item_height(rootId, page, i, pageReplyCount);
            if (h <= 0) break;
            if (y + h > CONTENT_BOTTOM)
                break;
            y += h + 4;
        }

        if (y + reply_view_item_height(rootId, page, target, pageReplyCount) <= CONTENT_BOTTOM)
            break;

        (*offset)++;
    }
}

static void draw_reply_view(
    const char *forumTitle,
    Post *root,
    int page,
    int selected,
    int offset,
    bool present
)
{
    int pageReplyCount = reply_page_item_count(root->id, page);
    int virtualCount = pageReplyCount + 1;
    int y = CONTENT_TOP;
    int virtualIndex;
    char pageText[24];

    gfx_SetDrawBuffer();
    gfx_FillScreen(COLOR_BACKGROUND);
    draw_header("REPLIES");

    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 1);
    gfx_PrintStringXY(forumTitle, 10, 47);
    gfx_SetTextScale(1, 1);

    for (virtualIndex = offset; virtualIndex < virtualCount; virtualIndex++)
    {
        if (y >= CONTENT_BOTTOM) break;

        if (virtualIndex == 0)
        {
            int h = post_height(root, false);
            draw_post(root, y, selected < 0, false);
            y += h + 4;
        }
        else
        {
            int position = reply_page_first_position(page) + virtualIndex - 1;
            int cacheIndex = reply_index_at_position(root->id, position);
            if (cacheIndex >= 0)
            {
                int h = post_height(&replyCache[cacheIndex], false);
                draw_post(&replyCache[cacheIndex], y, selected == cacheIndex, false);
                y += h + 4;
            }
        }
    }

    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(0, 218, 320, 22);
    sprintf(pageText, "Pg %d/%d", page + 1, reply_page_count(root->id));
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("^v Select", 5, 220);
    gfx_PrintStringXY("<> Page", 76, 220);
    gfx_PrintStringXY(pageText, 135, 220);
    gfx_PrintStringXY("ENTER Options", 190, 220);
    gfx_PrintStringXY("+ Reply", 5, 231);
    gfx_PrintStringXY("CLEAR Back", 244, 231);

    if (present)
    {
        gfx_BlitBuffer();
        gfx_SetDrawScreen();
    }
}

static int reply_menu(void)
{
    int option = 0;
    sk_key_t key;

    while (1)
    {
        gfx_SetDrawBuffer();
        gfx_SetColor(COLOR_PANEL_DARK);
        gfx_FillRectangle(52, 68, 216, 106);
        gfx_SetColor(COLOR_ACCENT);
        gfx_Rectangle(52, 68, 216, 106);
        gfx_SetTextFGColor(COLOR_TEXT);
        gfx_SetTextScale(2, 2);
        gfx_PrintStringXY("REPLY", 116, 76);
        gfx_SetTextScale(1, 1);

        gfx_SetColor(COLOR_SELECTED);
        gfx_FillRectangle(70, 112 + option * 22, 180, 18);
        gfx_SetTextFGColor(COLOR_TEXT);
        gfx_PrintStringXY("1  Like / Unlike", 82, 115);
        gfx_PrintStringXY("2  Reply", 82, 137);
        gfx_PrintStringXY("3  Cancel", 82, 159);

        gfx_BlitBuffer();
        gfx_SetDrawScreen();

        key = os_GetCSC();
        if (key == sk_Up)
        {
            if (option > 0) option--;
            else option = 2;
        }
        else if (key == sk_Down)
        {
            if (option < 2) option++;
            else option = 0;
        }
        else if (key == sk_Enter)
            return option;
        else if (key == sk_Clear)
            return 2;
    }
}

static void draw_sync_status(const char *line1, const char *line2, bool error)
{
    int w1;
    int w2;

    gfx_SetDrawBuffer();
    gfx_FillScreen(COLOR_BACKGROUND);

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 38);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("FORUMCE", 12, 7);
    gfx_SetTextScale(1, 1);

    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(38, 72, 244, 96);
    gfx_SetColor(error ? COLOR_PIN : COLOR_ACCENT);
    gfx_Rectangle(38, 72, 244, 96);

    gfx_SetTextFGColor(error ? COLOR_PIN : COLOR_ACCENT);
    w1 = gfx_GetStringWidth(line1);
    gfx_PrintStringXY(line1, (320 - w1) / 2, 98);

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    w2 = gfx_GetStringWidth(line2);
    gfx_PrintStringXY(line2, (320 - w2) / 2, 122);

    if (!error)
    {
        /* Simple activity mark; the screen stays stable while USB/server work. */
        gfx_SetColor(COLOR_ACCENT);
        gfx_FillCircle(145, 147, 2);
        gfx_FillCircle(160, 147, 2);
        gfx_FillCircle(175, 147, 2);
    }

    gfx_BlitBuffer();
    gfx_SetDrawScreen();
}

static void wait_after_sync_error(void)
{
    sk_key_t key;

    /* First wait for ENTER from the failed send to be released. */
    do
    {
        network_update();
        if (!network_usb_connected() || !network_server_online()) return;
        key = os_GetCSC();
    } while (key != 0);

    /* Then let the user acknowledge the failure with any key. */
    do
    {
        network_update();
        if (!network_usb_connected() || !network_server_online()) return;
        key = os_GetCSC();
    } while (key == 0);
}

static void draw_character_menu(void)
{
    gfx_SetDrawBuffer();
    gfx_SetColor(COLOR_PANEL_DARK);
    gfx_FillRectangle(18, 55, 284, 131);
    gfx_SetColor(COLOR_ACCENT);
    gfx_Rectangle(18, 55, 284, 131);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("CHARACTERS", 79, 64);
    gfx_SetTextScale(1, 1);

    gfx_PrintStringXY("1  Heart", 55, 108);
    gfx_PrintStringXY("2  Thumbs Up", 55, 130);
    gfx_PrintStringXY("3  Funny", 180, 108);
    gfx_PrintStringXY("4  Sad", 180, 130);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("CLEAR Cancel", 111, 164);
    gfx_BlitBuffer();
    gfx_SetDrawScreen();
}

static int character_menu(char *message, int *length)
{
    sk_key_t key;
    draw_character_menu();
    while (1)
    {
        key = os_GetCSC();
        if (key == sk_1) { if (*length < MAX_MESSAGE - 1) message[(*length)++] = CHAR_HEART; break; }
        if (key == sk_2) { if (*length < MAX_MESSAGE - 1) message[(*length)++] = CHAR_THUMBS; break; }
        if (key == sk_3) { if (*length < MAX_MESSAGE - 1) message[(*length)++] = CHAR_FUNNY; break; }
        if (key == sk_4) { if (*length < MAX_MESSAGE - 1) message[(*length)++] = CHAR_SAD; break; }
        if (key == sk_Clear) return 0;
    }
    message[*length] = '\0';
    return 1;
}

static int post_composer(bool specificReply, const Post *root, const Post *target)
{
    KeyboardState kb;
    sk_key_t key;
    int length = 0;
    int redraw = 1;

    composerMessage[0] = '\0';
    keyboard_reset(&kb);

    while (1)
    {
        if (redraw)
        {
            int lines;
            gfx_SetDrawBuffer();
            gfx_FillScreen(COLOR_BACKGROUND);
            draw_header(specificReply ? "REPLY" : "NEW POST");

            gfx_SetTextFGColor(COLOR_TEXT_MUTED);
            if (specificReply && target)
            {
                gfx_PrintStringXY("Replying to", 14, 49);
                if (strcmp(target->author, "Zytue67") == 0)
                    gfx_SetTextFGColor(COLOR_ZYTUE);
                else
                    gfx_SetTextFGColor(COLOR_ACCENT);
                gfx_PrintStringXY(target->author, 83, 49);
            }
            else
                gfx_PrintStringXY("Posting to this forum", 14, 49);

            gfx_SetColor(COLOR_PANEL);
            gfx_FillRectangle(10, 65, 300, 126);
            gfx_SetTextFGColor(COLOR_TEXT);
            draw_wrapped_text(composerMessage, 17, 72, 286);

            lines = get_line_count(composerMessage, 286);
            gfx_SetColor(COLOR_ACCENT);
            gfx_FillRectangle(17, 72 + lines * 9, 7, 1);

            gfx_SetColor(COLOR_PANEL_DARK);
            gfx_FillRectangle(0, 202, 320, 38);
            gfx_SetTextFGColor(COLOR_TEXT_MUTED);
            gfx_PrintStringXY("2nd+ALPHA Letters", 7, 206);
            gfx_PrintStringXY("ALPHA Case", 116, 206);
            gfx_PrintStringXY("MODE Chars", 201, 206);
            gfx_PrintStringXY("DEL Backspace", 7, 224);
            gfx_PrintStringXY("ENTER Send", 115, 224);
            gfx_PrintStringXY("CLEAR Cancel", 213, 224);

            gfx_BlitBuffer();
            gfx_SetDrawScreen();
            redraw = 0;
        }

        key = os_GetCSC();
        if (key == 0) continue;

        if (key == sk_Clear)
            return 0;
        if (key == sk_Del)
        {
            if (length > 0)
            {
                length--;
                composerMessage[length] = '\0';
                redraw = 1;
            }
            continue;
        }
        if (key == sk_Enter)
        {
            bool ok;
            if (length <= 0) continue;

            draw_sync_status("Sending...", "Please keep USB connected", false);
            if (specificReply && root)
                ok = network_create_reply(root->id, composerMessage);
            else
                ok = network_create_post(activeThreadId, composerMessage);

            if (!ok)
            {
                draw_sync_status("Send failed", "Press any key to continue", true);
                wait_after_sync_error();
                redraw = 1;
                continue;
            }

            draw_sync_status("Sent", "Syncing thread", false);
            if (specificReply && root)
                load_replies_for_root(root->id);
            else
                load_root_page(currentRootPage);
            return 1;
        }
        if (key == sk_Add)
        {
            character_menu(composerMessage, &length);
            redraw = 1;
            continue;
        }
        {
            char out;
            if (keyboard_handle_key(&kb, key, &out))
            {
                if (length < MAX_MESSAGE - 1)
                {
                    composerMessage[length++] = out;
                    composerMessage[length] = '\0';
                    redraw = 1;
                }
            }
        }
    }
}

static void like_root(Post *post)
{
    bool wasLiked = post->liked;
    if (!network_toggle_post_like(post->id)) return;
    post->liked = !wasLiked;
    if (post->liked) post->likes++;
    else if (post->likes > 0) post->likes--;
}

static void like_reply(Post *post)
{
    bool wasLiked = post->liked;
    if (!network_toggle_reply_like(post->id)) return;
    post->liked = !wasLiked;
    if (post->liked) post->likes++;
    else if (post->likes > 0) post->likes--;
}

static void reply_view(const char *forumTitle, Post *root)
{
    int selected = -1;
    int page = 0;
    int offset = 0;
    sk_key_t key;

    load_replies_for_root(root->id);

    while (1)
    {
        int totalReplies = reply_count_for_root(root->id);
        int pages = reply_page_count(root->id);
        int pageReplyCount;
        int first;

        if (page >= pages) page = pages - 1;
        if (page < 0) page = 0;
        pageReplyCount = reply_page_item_count(root->id, page);
        first = reply_page_first_position(page);

        if (selected >= 0)
        {
            int selectedPosition = reply_position_for_index(root->id, selected);
            if (selectedPosition < first || selectedPosition >= first + pageReplyCount)
                selected = pageReplyCount > 0 ? reply_index_at_position(root->id, first) : -1;
        }

        adjust_reply_offset(root->id, page, selected, pageReplyCount, &offset);
        draw_reply_view(forumTitle, root, page, selected, offset, true);
        key = os_GetCSC();

        if (key == sk_Up)
        {
            if (selected < 0)
            {
                if (page > 0)
                {
                    page--;
                    first = reply_page_first_position(page);
                    pageReplyCount = reply_page_item_count(root->id, page);
                    selected = pageReplyCount > 0
                        ? reply_index_at_position(root->id, first + pageReplyCount - 1)
                        : -1;
                    offset = 0;
                }
            }
            else
            {
                int pos = reply_position_for_index(root->id, selected);
                if (pos > first)
                    selected = reply_index_at_position(root->id, pos - 1);
                else
                    selected = -1;
            }
        }
        else if (key == sk_Down)
        {
            if (selected < 0)
            {
                if (pageReplyCount > 0)
                    selected = reply_index_at_position(root->id, first);
                else if (page + 1 < pages)
                {
                    page++;
                    first = reply_page_first_position(page);
                    pageReplyCount = reply_page_item_count(root->id, page);
                    selected = pageReplyCount > 0 ? reply_index_at_position(root->id, first) : -1;
                    offset = 0;
                }
            }
            else
            {
                int pos = reply_position_for_index(root->id, selected);
                if (pos + 1 < first + pageReplyCount)
                    selected = reply_index_at_position(root->id, pos + 1);
                else if (page + 1 < pages)
                {
                    page++;
                    first = reply_page_first_position(page);
                    pageReplyCount = reply_page_item_count(root->id, page);
                    selected = pageReplyCount > 0 ? reply_index_at_position(root->id, first) : -1;
                    offset = 0;
                }
            }
        }
        else if (key == sk_Left && page > 0)
        {
            page--;
            first = reply_page_first_position(page);
            pageReplyCount = reply_page_item_count(root->id, page);
            selected = pageReplyCount > 0 ? reply_index_at_position(root->id, first) : -1;
            offset = 0;
        }
        else if (key == sk_Right && page + 1 < pages)
        {
            page++;
            first = reply_page_first_position(page);
            pageReplyCount = reply_page_item_count(root->id, page);
            selected = pageReplyCount > 0 ? reply_index_at_position(root->id, first) : -1;
            offset = 0;
        }
        else if (key == sk_Enter)
        {
            if (selected < 0)
            {
                int action = action_menu(forumTitle, root, 0, 0);
                if (action == ACTION_REPLY)
                    post_composer(true, root, root);
                else if (action == ACTION_LIKE)
                    like_root(root);
            }
            else
            {
                int option = reply_menu();
                if (option == 0)
                    like_reply(&replyCache[selected]);
                else if (option == 1)
                    post_composer(true, root, &replyCache[selected]);
            }
        }
        else if (key == sk_Add)
        {
            post_composer(true, root, root);
        }
        else if (key == sk_Clear)
        {
            break;
        }
    }
}

void thread_screen(const char *forumTitle, int threadId)
{
    int selected = 0;
    int offset = 0;
    sk_key_t key;

    if (!initialize_thread(threadId))
    {
        draw_sync_status("Could not load thread", "Press any key to go back", true);
        wait_after_sync_error();
        return;
    }

    while (1)
    {
        int pageCount;
        network_update();
        if (!network_usb_connected() || !network_server_online()) break;
        pageCount = root_page_count();

        if (currentRootPage >= pageCount)
            currentRootPage = pageCount - 1;
        if (currentRootPage < 0)
            currentRootPage = 0;

        if (selected >= rootCacheCount)
            selected = rootCacheCount - 1;
        if (selected < 0)
            selected = 0;

        adjust_root_offset(selected, &offset);
        draw_thread(forumTitle, selected, offset, true);
        key = os_GetCSC();

        if (key == sk_Up)
        {
            if (selected > 0) selected--;
            else if (currentRootPage > 0)
            {
                if (load_root_page(currentRootPage - 1))
                {
                    selected = rootCacheCount > 0 ? rootCacheCount - 1 : 0;
                    offset = 0;
                }
            }
        }
        else if (key == sk_Down)
        {
            if (selected + 1 < rootCacheCount) selected++;
            else if (currentRootPage + 1 < pageCount)
            {
                if (load_root_page(currentRootPage + 1))
                {
                    selected = 0;
                    offset = 0;
                }
            }
        }
        else if (key == sk_Left && currentRootPage > 0)
        {
            if (load_root_page(currentRootPage - 1))
            {
                selected = 0;
                offset = 0;
            }
        }
        else if (key == sk_Right && currentRootPage + 1 < pageCount)
        {
            if (load_root_page(currentRootPage + 1))
            {
                selected = 0;
                offset = 0;
            }
        }
        else if (key == sk_Enter && rootCacheCount > 0)
        {
            Post *post = &rootCache[selected];
            int action = action_menu(forumTitle, post, selected, offset);

            if (action == ACTION_VIEW)
                reply_view(forumTitle, post);
            else if (action == ACTION_REPLY)
            {
                post_composer(true, post, post);
            }
            else if (action == ACTION_LIKE)
                like_root(post);
        }
        else if (key == sk_Add)
        {
            if (post_composer(false, NULL, NULL))
            {
                /* New posts are appended to the bottom of the conversation. */
                currentRootPage = root_page_count() - 1;
                if (load_root_page(currentRootPage))
                {
                    selected = rootCacheCount > 0 ? rootCacheCount - 1 : 0;
                    offset = 0;
                    adjust_root_offset(selected, &offset);
                }
            }
        }
        else if (key == sk_Clear)
            break;
    }
}
