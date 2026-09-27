#include <graphx.h>
#include <tice.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "connection.h"
#include "../network/network.h"
#include "../ui/theme.h"

static void draw_status_row(int y, const char *label, const char *value, bool ok)
{
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY(label, 52, y);

    gfx_SetTextFGColor(ok ? COLOR_SUCCESS : COLOR_TEXT);
    gfx_PrintStringXY(value, 154, y);
}

static void draw_screen(ForumCENetworkState state)
{
    const ForumCEUser *user = network_user();
    char account[28];

    gfx_FillScreen(COLOR_BACKGROUND);

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 0, 320, 44);
    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);
    gfx_PrintStringXY("FORUMCE", 94, 12);
    gfx_SetTextScale(1, 1);
    gfx_SetColor(COLOR_ACCENT);
    gfx_FillRectangle(0, 42, 320, 2);

    gfx_SetTextFGColor(COLOR_TEXT);
    gfx_SetTextScale(2, 2);

    if (state == FORUMCE_NET_AUTHENTICATED)
        gfx_PrintStringXY("CONNECTED", 100, 59);
    else if (state == FORUMCE_NET_SERVER_OFFLINE)
        gfx_PrintStringXY("SERVER OFFLINE", 70, 59);
    else if (state == FORUMCE_NET_AUTH_REQUIRED)
        gfx_PrintStringXY("SIGN IN NEEDED", 70, 59);
    else if (state == FORUMCE_NET_ERROR)
        gfx_PrintStringXY("CONNECTION ERROR", 62, 59);
    else
        gfx_PrintStringXY("CONNECTING...", 78, 59);

    gfx_SetTextScale(1, 1);

    draw_status_row(96, "USB", network_usb_connected() ? "Connected" : "Waiting...", network_usb_connected());
    draw_status_row(116, "Bridge", network_bridge_ready() ? "Ready" : "Waiting...", network_bridge_ready());
    draw_status_row(136, "Server", network_server_online() ? "Online" : "Waiting...", network_server_online());

    if (network_authenticated())
    {
        snprintf(account, sizeof account, "%s%s",
                 user->username,
                 user->creator ? " [Creator]" : "");
        draw_status_row(156, "Account", account, true);
    }
    else
    {
        draw_status_row(156, "Account", "Waiting...", false);
    }

    gfx_SetTextFGColor(COLOR_TEXT_MUTED);

    if (state == FORUMCE_NET_AUTHENTICATED)
    {
        gfx_PrintStringXY("ForumCE is ready.", 107, 187);
        gfx_SetTextFGColor(COLOR_TEXT);
        gfx_PrintStringXY("ENTER Continue", 112, 207);
    }
    else if (state == FORUMCE_NET_AUTH_REQUIRED)
    {
        gfx_PrintStringXY("Sign in on the ForumCE Bridge,", 63, 187);
        gfx_PrintStringXY("then restart the connection.", 75, 199);
    }
    else if (state == FORUMCE_NET_SERVER_OFFLINE)
    {
        gfx_PrintStringXY("Start the ForumCE server", 85, 187);
        gfx_PrintStringXY("and reconnect the bridge.", 78, 199);
    }
    else
    {
        gfx_PrintStringXY("Connect to the ForumCE Bridge.", 65, 187);
    }

    gfx_SetColor(COLOR_PANEL);
    gfx_FillRectangle(0, 220, 320, 20);
    gfx_SetTextFGColor(COLOR_TEXT_MUTED);
    gfx_PrintStringXY("CLEAR Exit", 132, 226);
}

bool startup_connection_screen(void)
{
    ForumCENetworkState last_state = (ForumCENetworkState)-1;
    bool last_usb = false;
    bool last_bridge = false;
    bool last_server = false;
    bool last_auth = false;

    while (1)
    {
        sk_key_t key;
        ForumCENetworkState state;

        network_update();
        state = network_state();

        if (state != last_state ||
            network_usb_connected() != last_usb ||
            network_bridge_ready() != last_bridge ||
            network_server_online() != last_server ||
            network_authenticated() != last_auth)
        {
            draw_screen(state);
            last_state = state;
            last_usb = network_usb_connected();
            last_bridge = network_bridge_ready();
            last_server = network_server_online();
            last_auth = network_authenticated();
        }

        key = os_GetCSC();

        if (key == sk_Clear)
            return false;

        if (key == sk_Enter && network_authenticated())
            return true;
    }
}
