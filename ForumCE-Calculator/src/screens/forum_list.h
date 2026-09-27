#ifndef FORUM_LIST_H
#define FORUM_LIST_H

typedef struct
{
    const char *title;
    const char *author;
    const char *lastResponse;
    long modified;
    int pinned;
    int forumId;
    int threadId;
} ForumItem;

int forum_list_screen(
    const char *categoryName,
    ForumItem *items,
    int count
);

#endif
