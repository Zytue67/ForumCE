#include <tice.h>
#include <graphx.h>
#include <stdio.h>
#include <string.h>

#include "settings.h"
#include "notifications.h"
#include "../ui/theme.h"
#include "../ui/keyboard.h"
#include "../network/network.h"

#define SETTINGS_COUNT 5
#define ACCOUNT_COUNT 4
#define NOTIFY_COUNT 5
#define BIO_EDIT_MAX 160

static char bioEdit[BIO_EDIT_MAX + 1];

static void draw_header(const char *title){gfx_FillScreen(COLOR_BACKGROUND);gfx_SetColor(COLOR_PANEL);gfx_FillRectangle(0,0,320,44);gfx_SetTextFGColor(COLOR_TEXT);gfx_SetTextScale(2,2);gfx_PrintStringXY(title,12,10);gfx_SetTextScale(1,1);gfx_SetColor(COLOR_ACCENT);gfx_FillRectangle(0,42,320,2);}
static void draw_footer(const char *text){gfx_SetColor(COLOR_PANEL);gfx_FillRectangle(0,220,320,20);gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY(text,160-gfx_GetStringWidth(text)/2,226);}
static void draw_menu_row(int y,const char *label,const char *value,int selected){gfx_SetColor(selected?COLOR_SELECTED:COLOR_BACKGROUND);gfx_FillRectangle(12,y-4,296,25);gfx_SetTextFGColor(selected?COLOR_SELECTED_TEXT:COLOR_TEXT);gfx_PrintStringXY(selected?">":" ",20,y+3);gfx_PrintStringXY(label,38,y+3);if(value){gfx_SetTextFGColor(selected?COLOR_SELECTED_TEXT:COLOR_TEXT_MUTED);gfx_PrintStringXY(value,298-gfx_GetStringWidth(value),y+3);}}
static void info_screen(const char *title,const char *line1,const char *line2){sk_key_t key;draw_header(title);gfx_SetTextFGColor(COLOR_TEXT);gfx_PrintStringXY(line1,160-gfx_GetStringWidth(line1)/2,92);gfx_SetTextFGColor(COLOR_TEXT_MUTED);if(line2)gfx_PrintStringXY(line2,160-gfx_GetStringWidth(line2)/2,111);draw_footer("CLEAR Back");while(1){network_update();key=os_GetCSC();if(key==sk_Clear)return;}}
static int confirm_twice(const char *title,const char *first,const char *second){int stage;sk_key_t key;for(stage=0;stage<2;stage++){const char *text=stage?second:first;draw_header(title);gfx_SetTextFGColor(COLOR_TEXT);gfx_PrintStringXY(text,160-gfx_GetStringWidth(text)/2,94);gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY("ENTER Confirm",56,137);gfx_PrintStringXY("CLEAR Cancel",186,137);while(1){network_update();key=os_GetCSC();if(key==sk_Clear)return 0;if(key==sk_Enter)break;}}return 1;}

static int edit_bio(void)
{
    const ForumCEUser *me=network_user();const ForumCESocialUser *profile;char out=0;int len,dirty=1;KeyboardState kb;sk_key_t key;
    if(!me||!network_load_profile(me->username))return 0;profile=network_profile();if(!profile)return 0;strncpy(bioEdit,profile->bio,BIO_EDIT_MAX);bioEdit[BIO_EDIT_MAX]=0;len=strlen(bioEdit);keyboard_reset(&kb);kb.mode=KEYBOARD_UPPER;while(os_GetCSC()!=0){}
    while(1){if(dirty){char count[20];int start,visible=43;draw_header("EDIT BIO");gfx_SetColor(COLOR_PANEL_DARK);gfx_FillRectangle(12,62,296,72);start=len>visible?len-visible:0;gfx_SetTextFGColor(COLOR_TEXT);gfx_PrintStringXY(bioEdit+start,20,79);sprintf(count,"%d/%d",len,BIO_EDIT_MAX);gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY(count,300-gfx_GetStringWidth(count),119);gfx_PrintStringXY(keyboard_mode_name(&kb),20,119);draw_footer("DEL Back  ENTER Save  CLEAR Cancel");dirty=0;}
        do{network_update();key=os_GetCSC();}while(key==0&&network_usb_connected());if(key==sk_Clear)return 0;if(key==sk_Del){if(len>0){bioEdit[--len]=0;dirty=1;}}
        else if(key==sk_Enter){draw_header("EDIT BIO");gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY("Saving to ForumCE...",98,105);if(network_update_bio(bioEdit))return 1;info_screen("SYNC FAILED","Bio was not changed.","Check the bridge and try again.");dirty=1;}
        else {KeyboardMode old=kb.mode;if(keyboard_handle_key(&kb,key,&out)&&out&&len<BIO_EDIT_MAX){bioEdit[len++]=out;bioEdit[len]=0;dirty=1;}else if(kb.mode!=old)dirty=1;}while(os_GetCSC()!=0){}
    }
}

static int account_screen(void)
{
    static const char *items[]={"Username","Edit Bio","Change Password","Log Out"};
    int selected=0;
    sk_key_t key;
    const ForumCEUser *me;
    int i;

    while(os_GetCSC()!=0){}
    me=network_user();
    draw_header("ACCOUNT");
    for(i=0;i<ACCOUNT_COUNT;i++)draw_menu_row(61+i*34,items[i],i==0&&me?me->username:NULL,i==selected);
    draw_footer("^v Select  ENTER Open  CLEAR Back");

    while(1)
    {
        int oldSelected=selected;
        me=network_user();
        do{network_update();key=os_GetCSC();}while(key==0&&network_usb_connected());
        if(key==sk_Clear)return 0;
        if(key==sk_Up&&selected>0)selected--;
        else if(key==sk_Down&&selected<ACCOUNT_COUNT-1)selected++;
        else if(key==sk_Enter)
        {
            if(selected==0)info_screen("USERNAME",me?me->username:"","Managed by your ForumCE account.");
            else if(selected==1)edit_bio();
            else if(selected==2)info_screen("PASSWORD","Use the ForumCE website","to change your password.");
            else if(selected==3&&confirm_twice("LOG OUT","Log out of ForumCE?","Are you really sure?"))
            {
                draw_header("LOG OUT");gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY("Signing out...",123,105);
                if(network_logout())return 1;
                info_screen("LOG OUT","Could not log out.","Check the bridge connection.");
            }
            me=network_user();
            draw_header("ACCOUNT");
            for(i=0;i<ACCOUNT_COUNT;i++)draw_menu_row(61+i*34,items[i],i==0&&me?me->username:NULL,i==selected);
            draw_footer("^v Select  ENTER Open  CLEAR Back");
        }
        if(selected!=oldSelected)
        {
            draw_menu_row(61+oldSelected*34,items[oldSelected],oldSelected==0&&me?me->username:NULL,0);
            draw_menu_row(61+selected*34,items[selected],selected==0&&me?me->username:NULL,1);
        }
        while(os_GetCSC()!=0){}
    }
}

static const char *on_off(int v){return v?"ON":"OFF";}
static void notifications_screen(void)
{
    static const char *items[]={"Notifications","Messages","Replies","Forum Activity","New Followers"};
    ForumCENotificationPrefs prefs,old;
    int selected=0;
    sk_key_t key;
    int i;

    if(!network_load_preferences(&prefs)){info_screen("NOTIFICATIONS","Could not load settings.","Check the ForumCE bridge.");return;}
    while(os_GetCSC()!=0){}

    draw_header("NOTIFICATIONS");
    for(i=0;i<NOTIFY_COUNT;i++)
    {
        int values[5]={prefs.master,prefs.messages,prefs.replies,prefs.forum_activity,prefs.new_followers};
        draw_menu_row(57+i*30,items[i],on_off(values[i]),i==selected);
    }
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);if(!prefs.master)gfx_PrintStringXY("Master OFF overrides preferences.",55,205);
    draw_footer("Y= Alerts  ENTER Toggle  CLEAR Back");

    while(1)
    {
        int oldSelected=selected;
        do{network_update();key=os_GetCSC();}while(key==0&&network_usb_connected());
        if(key==sk_Clear)return;
        if(key==sk_Yequ)
        {
            notification_feed_screen();
            draw_header("NOTIFICATIONS");
            {
                int values[5]={prefs.master,prefs.messages,prefs.replies,prefs.forum_activity,prefs.new_followers};
                for(i=0;i<NOTIFY_COUNT;i++)draw_menu_row(57+i*30,items[i],on_off(values[i]),i==selected);
            }
            gfx_SetTextFGColor(COLOR_TEXT_MUTED);if(!prefs.master)gfx_PrintStringXY("Master OFF overrides preferences.",55,205);
            draw_footer("Y= Alerts  ENTER Toggle  CLEAR Back");
            while(os_GetCSC()!=0){}
            continue;
        }
        if(key==sk_Up&&selected>0)selected--;
        else if(key==sk_Down&&selected<NOTIFY_COUNT-1)selected++;
        else if(key==sk_Enter)
        {
            int values[5];
            old=prefs;
            if(selected==0)prefs.master=!prefs.master;
            else if(selected==1)prefs.messages=!prefs.messages;
            else if(selected==2)prefs.replies=!prefs.replies;
            else if(selected==3)prefs.forum_activity=!prefs.forum_activity;
            else prefs.new_followers=!prefs.new_followers;
            if(!network_save_preferences(&prefs))
            {
                prefs=old;
                info_screen("SYNC FAILED","Preference was not changed.","Check the ForumCE bridge.");
                draw_header("NOTIFICATIONS");
                values[0]=prefs.master;values[1]=prefs.messages;values[2]=prefs.replies;values[3]=prefs.forum_activity;values[4]=prefs.new_followers;
                for(i=0;i<NOTIFY_COUNT;i++)draw_menu_row(57+i*30,items[i],on_off(values[i]),i==selected);
            }
            else
            {
                values[0]=prefs.master;values[1]=prefs.messages;values[2]=prefs.replies;values[3]=prefs.forum_activity;values[4]=prefs.new_followers;
                draw_menu_row(57+selected*30,items[selected],on_off(values[selected]),1);
            }
            gfx_SetColor(COLOR_BACKGROUND);gfx_FillRectangle(0,199,320,18);
            gfx_SetTextFGColor(COLOR_TEXT_MUTED);if(!prefs.master)gfx_PrintStringXY("Master OFF overrides preferences.",55,205);
            draw_footer("Y= Alerts  ENTER Toggle  CLEAR Back");
        }
        if(selected!=oldSelected)
        {
            int values[5]={prefs.master,prefs.messages,prefs.replies,prefs.forum_activity,prefs.new_followers};
            draw_menu_row(57+oldSelected*30,items[oldSelected],on_off(values[oldSelected]),0);
            draw_menu_row(57+selected*30,items[selected],on_off(values[selected]),1);
        }
        while(os_GetCSC()!=0){}
    }
}

static void connection_screen(void)
{
    sk_key_t key;const char *status=network_server_online()?"Online":"Offline";const char *usb=network_usb_connected()?"Connected":"Not Connected";const char *sync=network_clock_ready()?network_clock_time():"Just Now";draw_header("CONNECTION");draw_menu_row(62,"Status",status,0);draw_menu_row(94,"Last Sync",sync,0);draw_menu_row(126,"USB Connection",usb,0);gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY("USB sync is handled by the",70,171);gfx_PrintStringXY("ForumCE computer bridge.",75,187);draw_footer("CLEAR Back");while(1){network_update();key=os_GetCSC();if(key==sk_Clear)return;}}
static void about_screen(void){sk_key_t key;draw_header("ABOUT FORUMCE");gfx_SetTextFGColor(COLOR_TEXT);gfx_SetTextScale(2,2);gfx_PrintStringXY("FORUMCE V1.0",58,56);gfx_SetTextScale(1,1);gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY("A social forum built for the",55,87);gfx_PrintStringXY("TI-84 Plus CE: discuss, connect,",40,99);gfx_PrintStringXY("and message from your calculator.",42,111);gfx_SetTextFGColor(COLOR_TEXT);gfx_PrintStringXY("Created by Zytue67",91,132);gfx_SetTextFGColor(COLOR_TEXT_MUTED);gfx_PrintStringXY("Credits: CEdev / toolchain & libraries",35,151);gfx_PrintStringXY("OpenAI / ChatGPT - development help",37,163);gfx_PrintStringXY("(c) 2026 ForumCE",104,181);gfx_PrintStringXY("Website: Coming Soon",91,195);draw_footer("CLEAR Back");while(1){network_update();key=os_GetCSC();if(key==sk_Clear)return;}}
static void reset_local_data(void){if(!confirm_twice("RESET LOCAL DATA","Clear local ForumCE cache?","Online data will stay safe."))return;network_clear_social_cache();info_screen("RESET COMPLETE","Local cache was cleared.","Online account data was not deleted.");}

void settings_screen(void)
{
    static const char *items[]={"Account","Notifications","Connection","About ForumCE","Reset Local Data"};
    int selected=0;
    sk_key_t key;
    int i;

    while(os_GetCSC()!=0){}
    draw_header("SETTINGS");
    for(i=0;i<SETTINGS_COUNT;i++)draw_menu_row(57+i*31,items[i],NULL,i==selected);
    draw_footer("^v Select  ENTER Open  CLEAR Back");

    while(1)
    {
        int oldSelected=selected;
        do{network_update();key=os_GetCSC();}while(key==0&&network_usb_connected());
        if(key==sk_Clear)return;
        if(key==sk_Up&&selected>0)selected--;
        else if(key==sk_Down&&selected<SETTINGS_COUNT-1)selected++;
        else if(key==sk_Enter)
        {
            if(selected==0){if(account_screen())return;}
            else if(selected==1)notifications_screen();
            else if(selected==2)connection_screen();
            else if(selected==3)about_screen();
            else reset_local_data();
            if(!network_authenticated())return;
            draw_header("SETTINGS");
            for(i=0;i<SETTINGS_COUNT;i++)draw_menu_row(57+i*31,items[i],NULL,i==selected);
            draw_footer("^v Select  ENTER Open  CLEAR Back");
        }
        if(selected!=oldSelected)
        {
            draw_menu_row(57+oldSelected*31,items[oldSelected],NULL,0);
            draw_menu_row(57+selected*31,items[selected],NULL,1);
        }
        while(os_GetCSC()!=0){}
    }
}
