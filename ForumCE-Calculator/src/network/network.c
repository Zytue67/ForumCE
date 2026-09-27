#include <srldrvce.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usbdrvce.h>

#include "network.h"

static srl_device_t srl;
static uint8_t srl_buf[512];
static bool has_srl_device = false;
static bool bridge_ready = false;
static bool server_online = false;
static bool authenticated = false;
static bool usb_started = false;
static ForumCENetworkState state = FORUMCE_NET_WAITING;
static ForumCEUser current_user;

static char rx_line[768];
static unsigned int rx_len = 0;

typedef union {
    ForumCEForum forums[FORUMCE_MAX_FORUMS];
    ForumCEPost posts[FORUMCE_POSTS_PER_PAGE];
    ForumCEReply replies[FORUMCE_REPLIES_PER_PAGE];
    ForumCESocialUser users[FORUMCE_SOCIAL_USERS_PER_PAGE];
    ForumCEConversation conversations[FORUMCE_CONVS_PER_PAGE];
    ForumCEDirectMessage messages[FORUMCE_DM_WINDOW];
    ForumCENotification notifications[FORUMCE_NOTIFICATIONS_PER_PAGE];
} ForumCENetworkCache;
static ForumCENetworkCache net_cache;

static int forum_count;
static bool forums_loading, forums_ready;
static int post_count, post_total;
static bool posts_loading, posts_ready;
static int reply_net_count, reply_net_total;
static bool replies_loading, replies_ready;

static bool action_pending, action_success;
static bool request_pending, request_success;

static int social_user_count, social_total, social_database_total, social_page, social_pages;
static ForumCESocialUser profile_cache;
static bool profile_valid;
static int conversation_count, conversation_total, conversation_page, conversation_pages;
static int dm_count, dm_total, dm_offset;
static char dm_created_at[FORUMCE_STAMP_MAX];
static ForumCENotificationPrefs prefs_cache;
static int notification_count, notification_total, notification_page, notification_pages;
static int jump_page, jump_selected;

static bool clock_ready;
static char clock_date[6], clock_time[6];
static unsigned int clock_version;
static unsigned int unread_messages, unread_version;

#define ACTION_COMMAND_MAX (FORUMCE_POST_BODY_MAX + 48)
#define IO_TIMEOUT_SECONDS 8
#define WRITE_STALL_TIMEOUT_SECONDS 2
#define STATUS_CHECK_SECONDS 3
#define CLOCK_REFRESH_SECONDS 20
static char action_command[ACTION_COMMAND_MAX];
static clock_t last_status_check;
static clock_t last_clock_request;

static void clear_social_state(void)
{
    social_user_count = social_total = social_database_total = 0;
    social_page = 1; social_pages = 1;
    profile_valid = false;
    conversation_count = conversation_total = 0;
    conversation_page = 1; conversation_pages = 1;
    dm_count = dm_total = dm_offset = 0;
    dm_created_at[0] = '\0';
    jump_page = jump_selected = 0;
    memset(&prefs_cache, 0, sizeof prefs_cache);
    notification_count = notification_total = 0;
    notification_page = notification_pages = 1;
}

void network_clear_social_cache(void) { clear_social_state(); }

static void reset_session(void)
{
    bridge_ready = false; server_online = false; authenticated = false;
    memset(&current_user, 0, sizeof current_user);
    rx_len = 0;
    forums_loading = forums_ready = false; forum_count = 0;
    posts_loading = posts_ready = false; post_count = post_total = 0;
    replies_loading = replies_ready = false; reply_net_count = reply_net_total = 0;
    action_pending = action_success = false;
    request_pending = request_success = false;
    clear_social_state();
    clock_ready = false; clock_date[0] = clock_time[0] = '\0'; clock_version = 0;
    unread_messages = unread_version = 0;
    last_status_check = clock(); last_clock_request = 0;
}

static usb_error_t handle_usb_event(usb_event_t event, void *event_data,
    usb_callback_data_t *callback_data __attribute__((unused)))
{
    usb_error_t err = srl_UsbEventCallback(event, event_data, callback_data);
    if (err != USB_SUCCESS) return err;
    if (event == USB_DEVICE_CONNECTED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE))
    {
        usb_device_t device = event_data; usb_ResetDevice(device);
    }
    if (event == USB_HOST_CONFIGURE_EVENT ||
        (event == USB_DEVICE_ENABLED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE)))
    {
        usb_device_t device; srl_error_t error;
        if (has_srl_device) return USB_SUCCESS;
        if (event == USB_HOST_CONFIGURE_EVENT)
        {
            device = usb_FindDevice(NULL, NULL, USB_SKIP_HUBS);
            if (device == NULL) return USB_SUCCESS;
        }
        else device = event_data;
        error = srl_Open(&srl, device, srl_buf, sizeof srl_buf, SRL_INTERFACE_ANY, 9600);
        if (error) { state = FORUMCE_NET_ERROR; return USB_SUCCESS; }
        has_srl_device = true; reset_session(); state = FORUMCE_NET_WAITING;
    }
    if (event == USB_DEVICE_DISCONNECTED_EVENT)
    {
        usb_device_t device = event_data;
        if (has_srl_device && device == srl.dev)
        {
            srl_Close(&srl); has_srl_device = false; reset_session(); state = FORUMCE_NET_DISCONNECTED;
        }
    }
    return USB_SUCCESS;
}

static bool write_all(const char *data, size_t length)
{
    size_t sent = 0; clock_t last_progress = clock();
    if (!has_srl_device || !data) return false;
    while (sent < length)
    {
        int written = srl_Write(&srl, data + sent, length - sent);
        if (written < 0) return false;
        if (written > 0) { sent += (size_t)written; last_progress = clock(); }
        usb_HandleEvents();
        if (!has_srl_device) return false;
        if ((clock() - last_progress) >= (clock_t)(WRITE_STALL_TIMEOUT_SECONDS * CLOCKS_PER_SEC)) return false;
    }
    return true;
}

static bool send_line_reliable(const char *line)
{
    if (!has_srl_device || !line) return false;
    return write_all(line, strlen(line)) && write_all("\n", 1);
}

void network_send(const char *line) { (void)send_line_reliable(line); }

static void copy_text(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, size - 1); dst[size - 1] = '\0';
}

static void parse_user(char *line)
{
    char *id_text, *username, *role;
    strtok(line, "|"); id_text = strtok(NULL, "|"); username = strtok(NULL, "|"); role = strtok(NULL, "|");
    if (!id_text || !username || !role) { state = FORUMCE_NET_ERROR; return; }
    current_user.id = atoi(id_text); copy_text(current_user.username,sizeof current_user.username,username);
    copy_text(current_user.role,sizeof current_user.role,role); current_user.creator = strcmp(current_user.role,"CREATOR")==0;
    authenticated = true; state = FORUMCE_NET_AUTHENTICATED;
}

static void parse_forum(char *line)
{
    char *id_text,*thread_text,*name,*author,*stamp;
    strtok(line,"|"); id_text=strtok(NULL,"|"); thread_text=strtok(NULL,"|"); name=strtok(NULL,"|"); author=strtok(NULL,"|"); stamp=strtok(NULL,"|");
    if (!id_text || !thread_text || !name || forum_count >= FORUMCE_MAX_FORUMS) return;
    net_cache.forums[forum_count].id=atoi(id_text); net_cache.forums[forum_count].thread_id=atoi(thread_text);
    copy_text(net_cache.forums[forum_count].name,sizeof net_cache.forums[forum_count].name,name);
    copy_text(net_cache.forums[forum_count].author,sizeof net_cache.forums[forum_count].author,author?author:"ForumCE");
    copy_text(net_cache.forums[forum_count].last_activity,sizeof net_cache.forums[forum_count].last_activity,stamp);
    forum_count++;
}

static void parse_post(char *line)
{
    char *id_text,*author,*stamp,*likes,*liked,*replies,*body;
    strtok(line,"|"); id_text=strtok(NULL,"|"); author=strtok(NULL,"|"); stamp=strtok(NULL,"|"); likes=strtok(NULL,"|"); liked=strtok(NULL,"|"); replies=strtok(NULL,"|"); body=strtok(NULL,"|");
    if (!id_text || !author || !body || post_count >= FORUMCE_POSTS_PER_PAGE) return;
    net_cache.posts[post_count].id=atoi(id_text); copy_text(net_cache.posts[post_count].author,sizeof net_cache.posts[post_count].author,author);
    copy_text(net_cache.posts[post_count].created_at,sizeof net_cache.posts[post_count].created_at,stamp);
    net_cache.posts[post_count].likes=likes?atol(likes):0; net_cache.posts[post_count].liked=liked?atoi(liked)!=0:false; net_cache.posts[post_count].reply_count=replies?atoi(replies):0;
    copy_text(net_cache.posts[post_count].body,sizeof net_cache.posts[post_count].body,body); post_count++;
}

static void parse_reply(char *line)
{
    char *id_text,*post_text,*author,*stamp,*likes,*liked,*body;
    strtok(line,"|"); id_text=strtok(NULL,"|"); post_text=strtok(NULL,"|"); author=strtok(NULL,"|"); stamp=strtok(NULL,"|"); likes=strtok(NULL,"|"); liked=strtok(NULL,"|"); body=strtok(NULL,"|");
    if (!id_text || !post_text || !author || !body || reply_net_count >= FORUMCE_REPLIES_PER_PAGE) return;
    net_cache.replies[reply_net_count].id=atoi(id_text); net_cache.replies[reply_net_count].post_id=atoi(post_text);
    copy_text(net_cache.replies[reply_net_count].author,sizeof net_cache.replies[reply_net_count].author,author);
    copy_text(net_cache.replies[reply_net_count].created_at,sizeof net_cache.replies[reply_net_count].created_at,stamp);
    net_cache.replies[reply_net_count].likes=likes?atol(likes):0; net_cache.replies[reply_net_count].liked=liked?atoi(liked)!=0:false;
    copy_text(net_cache.replies[reply_net_count].body,sizeof net_cache.replies[reply_net_count].body,body); reply_net_count++;
}

static void parse_person(char *line, ForumCESocialUser *dst)
{
    char *id_text,*username,*followers,*following,*is_following,*bio;
    strtok(line,"|"); id_text=strtok(NULL,"|"); username=strtok(NULL,"|"); followers=strtok(NULL,"|"); following=strtok(NULL,"|"); is_following=strtok(NULL,"|"); bio=strtok(NULL,"|");
    if (!dst || !id_text || !username) return;
    memset(dst,0,sizeof *dst); dst->id=atoi(id_text); copy_text(dst->username,sizeof dst->username,username);
    dst->followers=followers?(unsigned int)strtoul(followers,NULL,10):0; dst->following=following?(unsigned int)strtoul(following,NULL,10):0;
    dst->is_following=is_following?atoi(is_following)!=0:false; copy_text(dst->bio,sizeof dst->bio,bio);
}

static void parse_conversation(char *line)
{
    char *cid,*uid,*username,*unread,*mine,*created,*last,*preview;
    if (conversation_count >= FORUMCE_CONVS_PER_PAGE) return;
    strtok(line,"|"); cid=strtok(NULL,"|"); uid=strtok(NULL,"|"); username=strtok(NULL,"|"); unread=strtok(NULL,"|"); mine=strtok(NULL,"|"); created=strtok(NULL,"|"); last=strtok(NULL,"|"); preview=strtok(NULL,"|");
    if (!cid || !uid || !username) return;
    net_cache.conversations[conversation_count].conversation_id=atoi(cid); net_cache.conversations[conversation_count].user_id=atoi(uid);
    copy_text(net_cache.conversations[conversation_count].username,sizeof net_cache.conversations[conversation_count].username,username);
    net_cache.conversations[conversation_count].unread=unread?(unsigned int)strtoul(unread,NULL,10):0; net_cache.conversations[conversation_count].last_mine=mine?atoi(mine)!=0:false;
    copy_text(net_cache.conversations[conversation_count].created_at,sizeof net_cache.conversations[conversation_count].created_at,created);
    copy_text(net_cache.conversations[conversation_count].last_activity,sizeof net_cache.conversations[conversation_count].last_activity,last);
    copy_text(net_cache.conversations[conversation_count].preview,sizeof net_cache.conversations[conversation_count].preview,preview); conversation_count++;
}

static void parse_dm(char *line)
{
    char *id,*sender,*mine,*username,*stamp,*body;
    if (dm_count >= FORUMCE_DM_WINDOW) return;
    strtok(line,"|"); id=strtok(NULL,"|"); sender=strtok(NULL,"|"); mine=strtok(NULL,"|"); username=strtok(NULL,"|"); stamp=strtok(NULL,"|"); body=strtok(NULL,"|");
    if (!id || !sender) return;
    net_cache.messages[dm_count].id=atoi(id); net_cache.messages[dm_count].sender_id=atoi(sender); net_cache.messages[dm_count].mine=mine?atoi(mine)!=0:false;
    copy_text(net_cache.messages[dm_count].username,sizeof net_cache.messages[dm_count].username,username);
    copy_text(net_cache.messages[dm_count].created_at,sizeof net_cache.messages[dm_count].created_at,stamp);
    copy_text(net_cache.messages[dm_count].body,sizeof net_cache.messages[dm_count].body,body); dm_count++;
}

static void parse_notification(char *line)
{
    char *id,*read_text,*type,*actor,*stamp,*text;
    if (notification_count >= FORUMCE_NOTIFICATIONS_PER_PAGE) return;
    strtok(line,"|"); id=strtok(NULL,"|"); read_text=strtok(NULL,"|"); type=strtok(NULL,"|"); actor=strtok(NULL,"|"); stamp=strtok(NULL,"|"); text=strtok(NULL,"|");
    if (!id || !text) return;
    net_cache.notifications[notification_count].id=atoi(id);
    net_cache.notifications[notification_count].unread=read_text?atoi(read_text)==0:false;
    copy_text(net_cache.notifications[notification_count].type,sizeof net_cache.notifications[notification_count].type,type);
    copy_text(net_cache.notifications[notification_count].actor,sizeof net_cache.notifications[notification_count].actor,actor);
    copy_text(net_cache.notifications[notification_count].created_at,sizeof net_cache.notifications[notification_count].created_at,stamp);
    copy_text(net_cache.notifications[notification_count].text,sizeof net_cache.notifications[notification_count].text,text);
    notification_count++;
}

static void finish_request(bool success) { request_success=success; request_pending=false; }

static void handle_line(char *line)
{
    if (strcmp(line,"READY")==0) { bridge_ready=true; state=FORUMCE_NET_BRIDGE_READY; network_send("STATUS"); }
    else if (strcmp(line,"ONLINE")==0) { server_online=true; if(authenticated) state=FORUMCE_NET_AUTHENTICATED; else { state=FORUMCE_NET_SERVER_ONLINE; network_send("ME"); } }
    else if (strcmp(line,"OFFLINE")==0) { server_online=false; state=FORUMCE_NET_SERVER_OFFLINE; action_pending=false; request_pending=false; }
    else if (strcmp(line,"AUTH_REQUIRED")==0) { authenticated=false; state=FORUMCE_NET_AUTH_REQUIRED; }
    else if (strncmp(line,"USER|",5)==0) parse_user(line);
    else if (strncmp(line,"FORUM_COUNT|",12)==0) { forum_count=0; forums_loading=true; forums_ready=false; }
    else if (strncmp(line,"FORUM|",6)==0) parse_forum(line);
    else if (strcmp(line,"FORUMS_END")==0) { forums_loading=false; forums_ready=true; }
    else if (strncmp(line,"POST_COUNT|",11)==0) { char *t; strtok(line,"|"); t=strtok(NULL,"|"); post_total=t?atoi(t):0; post_count=0; posts_loading=true; posts_ready=false; }
    else if (strncmp(line,"POST|",5)==0) parse_post(line);
    else if (strcmp(line,"POSTS_END")==0) { posts_loading=false; posts_ready=true; }
    else if (strncmp(line,"REPLY_COUNT|",12)==0) { char *t; strtok(line,"|"); t=strtok(NULL,"|"); reply_net_total=t?atoi(t):0; reply_net_count=0; replies_loading=true; replies_ready=false; }
    else if (strncmp(line,"REPLY|",6)==0) parse_reply(line);
    else if (strcmp(line,"REPLIES_END")==0) { replies_loading=false; replies_ready=true; }
    else if (strcmp(line,"ACTION_OK")==0) { action_success=true; action_pending=false; }
    else if (strncmp(line,"ACTION_ERR",10)==0) { action_success=false; action_pending=false; }
    else if (strncmp(line,"CLOCK|",6)==0) { char *d,*t; strtok(line,"|"); d=strtok(NULL,"|"); t=strtok(NULL,"|"); if(d&&t){copy_text(clock_date,sizeof clock_date,d);copy_text(clock_time,sizeof clock_time,t);clock_ready=true;clock_version++;} }
    else if (strncmp(line,"UNREAD|",7)==0) { unread_messages=(unsigned int)strtoul(line+7,NULL,10); unread_version++; }
    else if (strncmp(line,"PEOPLE_COUNT|",13)==0) { char *a,*b,*c,*d; strtok(line,"|");a=strtok(NULL,"|");b=strtok(NULL,"|");c=strtok(NULL,"|");d=strtok(NULL,"|"); social_total=a?atoi(a):0;social_page=b?atoi(b):1;social_pages=c?atoi(c):1;social_database_total=d?atoi(d):social_total;social_user_count=0; }
    else if (strncmp(line,"LIST_COUNT|",11)==0) { char *a,*b,*c; strtok(line,"|");a=strtok(NULL,"|");b=strtok(NULL,"|");c=strtok(NULL,"|");social_total=a?atoi(a):0;social_page=b?atoi(b):1;social_pages=c?atoi(c):1;social_database_total=social_total;social_user_count=0; }
    else if (strncmp(line,"SEARCH_COUNT|",13)==0) { social_total=atoi(line+13); social_page=1;social_pages=1;social_database_total=social_total;social_user_count=0; }
    else if (strncmp(line,"PERSON|",7)==0) { if(social_user_count<FORUMCE_SOCIAL_USERS_PER_PAGE){parse_person(line,&net_cache.users[social_user_count]);social_user_count++;} }
    else if (strcmp(line,"PEOPLE_END")==0 || strcmp(line,"LIST_END")==0 || strcmp(line,"SEARCH_END")==0) finish_request(true);
    else if (strncmp(line,"PROFILEDATA|",12)==0) { parse_person(line,&profile_cache); profile_valid=true; finish_request(true); }
    else if (strncmp(line,"JUMP|",5)==0) { char *p,*s;strtok(line,"|");p=strtok(NULL,"|");s=strtok(NULL,"|");jump_page=p?atoi(p):0;jump_selected=s?atoi(s):0;finish_request(jump_page>0); }
    else if (strncmp(line,"CONV_COUNT|",11)==0) { char *a,*b,*c,*u;strtok(line,"|");a=strtok(NULL,"|");b=strtok(NULL,"|");c=strtok(NULL,"|");u=strtok(NULL,"|");conversation_total=a?atoi(a):0;conversation_page=b?atoi(b):1;conversation_pages=c?atoi(c):1;unread_messages=u?(unsigned int)strtoul(u,NULL,10):0;unread_version++;conversation_count=0; }
    else if (strncmp(line,"CONV|",5)==0) parse_conversation(line);
    else if (strcmp(line,"CONVS_END")==0) finish_request(true);
    else if (strncmp(line,"DM_COUNT|",9)==0) { char *a,*b,*c;strtok(line,"|");a=strtok(NULL,"|");b=strtok(NULL,"|");c=strtok(NULL,"|");dm_total=a?atoi(a):0;dm_offset=b?atoi(b):0;copy_text(dm_created_at,sizeof dm_created_at,c);dm_count=0; }
    else if (strncmp(line,"DM|",3)==0) parse_dm(line);
    else if (strcmp(line,"DMS_END")==0) finish_request(true);
    else if (strncmp(line,"NOTIF_COUNT|",12)==0) { char *a,*b,*c;strtok(line,"|");a=strtok(NULL,"|");b=strtok(NULL,"|");c=strtok(NULL,"|");notification_total=a?atoi(a):0;notification_page=b?atoi(b):1;notification_pages=c?atoi(c):1;notification_count=0; }
    else if (strncmp(line,"NOTIF|",6)==0) parse_notification(line);
    else if (strcmp(line,"NOTIFS_END")==0) finish_request(true);
    else if (strncmp(line,"PREFS|",6)==0) { char *a,*b,*c,*d,*e;strtok(line,"|");a=strtok(NULL,"|");b=strtok(NULL,"|");c=strtok(NULL,"|");d=strtok(NULL,"|");e=strtok(NULL,"|");prefs_cache.master=a?atoi(a)!=0:false;prefs_cache.messages=b?atoi(b)!=0:false;prefs_cache.replies=c?atoi(c)!=0:false;prefs_cache.forum_activity=d?atoi(d)!=0:false;prefs_cache.new_followers=e?atoi(e)!=0:false;finish_request(true); }
    else if (strncmp(line,"ERR|",4)==0) {
        if(strcmp(line,"ERR|POSTS")==0){posts_loading=false;posts_ready=false;}
        else if(strcmp(line,"ERR|FORUMS")==0){forums_loading=false;forums_ready=false;}
        else if(strcmp(line,"ERR|REPLIES")==0){replies_loading=false;replies_ready=false;}
        else finish_request(false);
    }
}

bool network_init(void)
{
    const usb_standard_descriptors_t *desc; usb_error_t error;
    memset(&current_user,0,sizeof current_user); reset_session(); state=FORUMCE_NET_WAITING;
    desc=srl_GetCDCStandardDescriptors(); error=usb_Init(handle_usb_event,NULL,desc,USB_DEFAULT_INIT_FLAGS);
    if(error!=USB_SUCCESS){usb_Cleanup();state=FORUMCE_NET_ERROR;return false;} usb_started=true;return true;
}

void network_update(void)
{
    char chunk[64]; int count; size_t i;
    if(!usb_started)return; usb_HandleEvents(); if(!has_srl_device)return;
    if(bridge_ready && !action_pending && !request_pending && (clock()-last_status_check)>=(clock_t)(STATUS_CHECK_SECONDS*CLOCKS_PER_SEC))
    { last_status_check=clock(); (void)send_line_reliable("STATUS"); }
    count=srl_Read(&srl,chunk,sizeof chunk); if(count<=0)return;
    for(i=0;i<(size_t)count;i++){
        char c=chunk[i]; if(c=='\r')continue;
        if(c=='\n'){if(rx_len>0){rx_line[rx_len]='\0';handle_line(rx_line);rx_len=0;}continue;}
        if(rx_len<sizeof rx_line-1)rx_line[rx_len++]=c; else rx_len=0;
    }
}

void network_cleanup(void)
{
    if(!usb_started)return; if(has_srl_device){srl_Close(&srl);has_srl_device=false;} usb_Cleanup();usb_started=false;reset_session();
}

ForumCENetworkState network_state(void){return state;} bool network_usb_connected(void){return has_srl_device;} bool network_bridge_ready(void){return bridge_ready;} bool network_server_online(void){return server_online;} bool network_authenticated(void){return authenticated;} const ForumCEUser *network_user(void){return &current_user;}

void network_request_forums(void){forum_count=0;forums_loading=true;forums_ready=false;if(!send_line_reliable("FORUMS"))forums_loading=false;}
bool network_forums_loading(void){return forums_loading;} bool network_forums_ready(void){return forums_ready;} int network_forum_count(void){return forum_count;} const ForumCEForum *network_forum(int i){return(i>=0&&i<forum_count)?&net_cache.forums[i]:NULL;}
void network_request_posts(int thread_id,int page){char c[40];post_count=post_total=0;posts_loading=true;posts_ready=false;sprintf(c,"POSTS|%d|%d",thread_id,page);if(!send_line_reliable(c))posts_loading=false;}
bool network_posts_loading(void){return posts_loading;} bool network_posts_ready(void){return posts_ready;} int network_post_total(void){return post_total;} int network_post_count(void){return post_count;} const ForumCEPost *network_post(int i){return(i>=0&&i<post_count)?&net_cache.posts[i]:NULL;}
void network_request_replies(int post_id,int page){char c[40];reply_net_count=reply_net_total=0;replies_loading=true;replies_ready=false;sprintf(c,"REPLIES|%d|%d",post_id,page);if(!send_line_reliable(c))replies_loading=false;}
bool network_replies_loading(void){return replies_loading;} bool network_replies_ready(void){return replies_ready;} int network_reply_total(void){return reply_net_total;} int network_reply_count(void){return reply_net_count;} const ForumCEReply *network_reply(int i){return(i>=0&&i<reply_net_count)?&net_cache.replies[i]:NULL;}

static bool wait_flag(bool *pending,bool *success)
{
    clock_t started=clock();
    while(*pending && network_usb_connected() && network_server_online()){
        network_update();
        if((clock()-started)>=(clock_t)(IO_TIMEOUT_SECONDS*CLOCKS_PER_SEC)){*pending=false;*success=false;break;}
    }
    return !*pending && *success;
}
static bool send_action(const char *command)
{
    if(!network_usb_connected()||!network_server_online()||!command)return false;
    action_pending=true;action_success=false;if(!send_line_reliable(command)){action_pending=false;return false;}return wait_flag(&action_pending,&action_success);
}
static bool send_request(const char *command)
{
    if(!network_usb_connected()||!network_server_online()||!network_authenticated()||!command)return false;
    request_pending=true;request_success=false;if(!send_line_reliable(command)){request_pending=false;return false;}return wait_flag(&request_pending,&request_success);
}

bool network_create_post(int id,const char *body){if(!body)return false;snprintf(action_command,sizeof action_command,"NEWPOST|%d|%s",id,body);return send_action(action_command);}
bool network_create_reply(int id,const char *body){if(!body)return false;snprintf(action_command,sizeof action_command,"NEWREPLY|%d|%s",id,body);return send_action(action_command);}
bool network_toggle_post_like(int id){snprintf(action_command,sizeof action_command,"LIKEPOST|%d",id);return send_action(action_command);}
bool network_toggle_reply_like(int id){snprintf(action_command,sizeof action_command,"LIKEREPLY|%d",id);return send_action(action_command);}
bool network_create_forum(const char *name){if(!name)return false;snprintf(action_command,sizeof action_command,"NEWFORUM|%s",name);return send_action(action_command);}
bool network_delete_forum(int id){if(id<=1)return false;snprintf(action_command,sizeof action_command,"DELETEFORUM|%d",id);return send_action(action_command);}

void network_request_clock(void){if(!action_pending&&!request_pending&&has_srl_device){last_clock_request=clock();(void)send_line_reliable("TIME");}}
void network_maybe_request_clock(void){if(!action_pending&&!request_pending&&has_srl_device&&authenticated&&(last_clock_request==0||(clock()-last_clock_request)>=(clock_t)(CLOCK_REFRESH_SECONDS*CLOCKS_PER_SEC)))network_request_clock();}
bool network_clock_ready(void){return clock_ready;} const char *network_clock_date(void){return clock_date;} const char *network_clock_time(void){return clock_time;} unsigned int network_clock_version(void){return clock_version;}
void network_request_unread(void){if(!action_pending&&!request_pending&&has_srl_device&&authenticated)(void)send_line_reliable("UNREAD");}
unsigned int network_unread_messages(void){return unread_messages;} unsigned int network_unread_version(void){return unread_version;}

bool network_load_people(int page){snprintf(action_command,sizeof action_command,"PEOPLE|%d",page);return send_request(action_command);}
bool network_jump_people(char letter,int *page,int *selected){snprintf(action_command,sizeof action_command,"PEOPLEJUMP|%c",letter);if(!send_request(action_command))return false;if(page)*page=jump_page;if(selected)*selected=jump_selected;return true;}
bool network_load_relation(const char *username,bool following,int page){if(!username)return false;snprintf(action_command,sizeof action_command,"%s|%s|%d",following?"FOLLOWING":"FOLLOWERS",username,page);return send_request(action_command);}
bool network_jump_relation(const char *username,bool following,char letter,int *page,int *selected){if(!username)return false;snprintf(action_command,sizeof action_command,"RELJUMP|%c|%s|%c",following?'G':'F',username,letter);if(!send_request(action_command))return false;if(page)*page=jump_page;if(selected)*selected=jump_selected;return true;}
bool network_search_users(const char *query){if(!query||!query[0]){social_user_count=social_total=0;return true;}snprintf(action_command,sizeof action_command,"SEARCH|%s",query);return send_request(action_command);}
int network_social_user_count(void){return social_user_count;} int network_social_total(void){return social_total;} int network_social_database_total(void){return social_database_total;} int network_social_page(void){return social_page;} int network_social_pages(void){return social_pages;} const ForumCESocialUser *network_social_user(int i){return(i>=0&&i<social_user_count)?&net_cache.users[i]:NULL;}

bool network_load_profile(const char *username){if(!username)return false;profile_valid=false;snprintf(action_command,sizeof action_command,"PROFILE|%s",username);return send_request(action_command)&&profile_valid;}
const ForumCESocialUser *network_profile(void){return profile_valid?&profile_cache:NULL;}
bool network_set_follow(const char *username,bool follow){if(!username)return false;snprintf(action_command,sizeof action_command,"FOLLOW|%s|%d",username,follow?1:0);return send_action(action_command);}
bool network_update_bio(const char *bio){if(!bio)return false;snprintf(action_command,sizeof action_command,"BIO|%s",bio);return send_action(action_command);}

bool network_load_conversations(int page){snprintf(action_command,sizeof action_command,"CONVS|%d",page);return send_request(action_command);}
int network_conversation_count(void){return conversation_count;} int network_conversation_total(void){return conversation_total;} int network_conversation_page(void){return conversation_page;} int network_conversation_pages(void){return conversation_pages;} const ForumCEConversation *network_conversation(int i){return(i>=0&&i<conversation_count)?&net_cache.conversations[i]:NULL;}
bool network_load_dm_window(const char *username,int offset){if(!username)return false;snprintf(action_command,sizeof action_command,"DMS|%s|%d",username,offset);return send_request(action_command);}
int network_dm_count(void){return dm_count;} int network_dm_total(void){return dm_total;} int network_dm_offset(void){return dm_offset;} const char *network_dm_created_at(void){return dm_created_at;} const ForumCEDirectMessage *network_dm(int i){return(i>=0&&i<dm_count)?&net_cache.messages[i]:NULL;}
bool network_send_dm(const char *username,const char *body){if(!username||!body)return false;snprintf(action_command,sizeof action_command,"SENDDM|%s|%s",username,body);return send_action(action_command);}
bool network_mark_dm_read(const char *username){if(!username)return false;snprintf(action_command,sizeof action_command,"READDM|%s",username);return send_action(action_command);}
bool network_delete_dm(const char *username){if(!username)return false;snprintf(action_command,sizeof action_command,"DELETEDM|%s",username);return send_action(action_command);}

bool network_load_notifications(int page){snprintf(action_command,sizeof action_command,"NOTIFS|%d",page);return send_request(action_command);}
int network_notification_count(void){return notification_count;}
int network_notification_total(void){return notification_total;}
int network_notification_page(void){return notification_page;}
int network_notification_pages(void){return notification_pages;}
const ForumCENotification *network_notification(int i){return(i>=0&&i<notification_count)?&net_cache.notifications[i]:NULL;}
bool network_mark_notifications_read(void){return send_action("NOTIFSREAD");}

bool network_load_preferences(ForumCENotificationPrefs *out){if(!send_request("PREFS"))return false;if(out)*out=prefs_cache;return true;}
bool network_save_preferences(const ForumCENotificationPrefs *p){if(!p)return false;snprintf(action_command,sizeof action_command,"SETPREFS|%d|%d|%d|%d|%d",p->master,p->messages,p->replies,p->forum_activity,p->new_followers);return send_action(action_command);}
bool network_logout(void){bool ok=send_action("LOGOUT");if(ok){authenticated=false;state=FORUMCE_NET_AUTH_REQUIRED;}return ok;}
