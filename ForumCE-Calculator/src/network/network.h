#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdint.h>

#define FORUMCE_MAX_FORUMS 32
#define FORUMCE_FORUM_NAME_MAX 32
#define FORUMCE_AUTHOR_MAX 21
#define FORUMCE_STAMP_MAX 17
#define FORUMCE_POST_BODY_MAX 320
#define FORUMCE_POSTS_PER_PAGE 10
#define FORUMCE_REPLIES_PER_PAGE 10

#define FORUMCE_SOCIAL_USERS_PER_PAGE 10
#define FORUMCE_CONVS_PER_PAGE 6
#define FORUMCE_DM_WINDOW 3
#define FORUMCE_USERNAME_MAX 21
#define FORUMCE_BIO_MAX 161
#define FORUMCE_DM_BODY_MAX 161
#define FORUMCE_PREVIEW_MAX 45
#define FORUMCE_NOTIFICATIONS_PER_PAGE 10
#define FORUMCE_NOTIFICATION_TEXT_MAX 96
#define FORUMCE_NOTIFICATION_TYPE_MAX 12

typedef enum {
    FORUMCE_NET_WAITING = 0,
    FORUMCE_NET_BRIDGE_READY,
    FORUMCE_NET_SERVER_ONLINE,
    FORUMCE_NET_AUTHENTICATED,
    FORUMCE_NET_SERVER_OFFLINE,
    FORUMCE_NET_AUTH_REQUIRED,
    FORUMCE_NET_DISCONNECTED,
    FORUMCE_NET_ERROR
} ForumCENetworkState;

typedef struct { int id; char username[21]; char role[12]; bool creator; } ForumCEUser;
typedef struct {
    int id;
    int thread_id;
    char name[FORUMCE_FORUM_NAME_MAX];
    char author[FORUMCE_AUTHOR_MAX];
    char last_activity[FORUMCE_STAMP_MAX];
} ForumCEForum;
typedef struct {
    int id;
    char author[FORUMCE_AUTHOR_MAX];
    char created_at[FORUMCE_STAMP_MAX];
    long likes;
    bool liked;
    int reply_count;
    char body[FORUMCE_POST_BODY_MAX];
} ForumCEPost;
typedef struct {
    int id;
    int post_id;
    char author[FORUMCE_AUTHOR_MAX];
    char created_at[FORUMCE_STAMP_MAX];
    long likes;
    bool liked;
    char body[FORUMCE_POST_BODY_MAX];
} ForumCEReply;

typedef struct {
    int id;
    char username[FORUMCE_USERNAME_MAX];
    char bio[FORUMCE_BIO_MAX];
    unsigned int followers;
    unsigned int following;
    bool is_following;
} ForumCESocialUser;

typedef struct {
    int conversation_id;
    int user_id;
    char username[FORUMCE_USERNAME_MAX];
    unsigned int unread;
    bool last_mine;
    char created_at[FORUMCE_STAMP_MAX];
    char last_activity[FORUMCE_STAMP_MAX];
    char preview[FORUMCE_PREVIEW_MAX];
} ForumCEConversation;

typedef struct {
    int id;
    int sender_id;
    bool mine;
    char username[FORUMCE_USERNAME_MAX];
    char created_at[FORUMCE_STAMP_MAX];
    char body[FORUMCE_DM_BODY_MAX];
} ForumCEDirectMessage;


typedef struct {
    int id;
    bool unread;
    char type[FORUMCE_NOTIFICATION_TYPE_MAX];
    char actor[FORUMCE_USERNAME_MAX];
    char created_at[FORUMCE_STAMP_MAX];
    char text[FORUMCE_NOTIFICATION_TEXT_MAX];
} ForumCENotification;

typedef struct {
    bool master;
    bool messages;
    bool replies;
    bool forum_activity;
    bool new_followers;
} ForumCENotificationPrefs;

bool network_init(void);
void network_update(void);
void network_cleanup(void);
void network_send(const char *line);
ForumCENetworkState network_state(void);
bool network_usb_connected(void);
bool network_bridge_ready(void);
bool network_server_online(void);
bool network_authenticated(void);
const ForumCEUser *network_user(void);

void network_request_forums(void);
bool network_forums_loading(void);
bool network_forums_ready(void);
int network_forum_count(void);
const ForumCEForum *network_forum(int index);

void network_request_posts(int thread_id, int page);
bool network_posts_loading(void);
bool network_posts_ready(void);
int network_post_total(void);
int network_post_count(void);
const ForumCEPost *network_post(int index);

void network_request_replies(int post_id, int page);
bool network_replies_loading(void);
bool network_replies_ready(void);
int network_reply_total(void);
int network_reply_count(void);
const ForumCEReply *network_reply(int index);

bool network_create_post(int thread_id, const char *body);
bool network_create_reply(int post_id, const char *body);
bool network_toggle_post_like(int post_id);
bool network_toggle_reply_like(int reply_id);
bool network_create_forum(const char *name);
bool network_delete_forum(int forum_id);

void network_request_clock(void);
void network_maybe_request_clock(void);
bool network_clock_ready(void);
const char *network_clock_date(void);
const char *network_clock_time(void);
unsigned int network_clock_version(void);

void network_request_unread(void);
unsigned int network_unread_messages(void);
unsigned int network_unread_version(void);

bool network_load_people(int page);
bool network_jump_people(char letter, int *page, int *selected);
bool network_load_relation(const char *username, bool following, int page);
bool network_jump_relation(const char *username, bool following, char letter, int *page, int *selected);
bool network_search_users(const char *query);
int network_social_user_count(void);
int network_social_total(void);
int network_social_database_total(void);
int network_social_page(void);
int network_social_pages(void);
const ForumCESocialUser *network_social_user(int index);

bool network_load_profile(const char *username);
const ForumCESocialUser *network_profile(void);

bool network_set_follow(const char *username, bool follow);
bool network_update_bio(const char *bio);

bool network_load_conversations(int page);
int network_conversation_count(void);
int network_conversation_total(void);
int network_conversation_page(void);
int network_conversation_pages(void);
const ForumCEConversation *network_conversation(int index);

bool network_load_dm_window(const char *username, int offset);
int network_dm_count(void);
int network_dm_total(void);
int network_dm_offset(void);
const char *network_dm_created_at(void);
const ForumCEDirectMessage *network_dm(int index);
bool network_send_dm(const char *username, const char *body);
bool network_mark_dm_read(const char *username);
bool network_delete_dm(const char *username);

bool network_load_notifications(int page);
int network_notification_count(void);
int network_notification_total(void);
int network_notification_page(void);
int network_notification_pages(void);
const ForumCENotification *network_notification(int index);
bool network_mark_notifications_read(void);

bool network_load_preferences(ForumCENotificationPrefs *out);
bool network_save_preferences(const ForumCENotificationPrefs *prefs);
bool network_logout(void);
void network_clear_social_cache(void);

#endif
