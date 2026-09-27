#include <tice.h>
#include <graphx.h>

#include "ui/ui.h"
#include "network/network.h"
#include "screens/connection.h"
#include "screens/home.h"

int main(void)
{
    bool continue_to_home;

    gfx_Begin();
    ui_init();

    if (!network_init())
    {
        startup_connection_screen();
        gfx_End();
        return 1;
    }

    while (1)
    {
        continue_to_home = startup_connection_screen();
        if (!continue_to_home) break;
        home_screen();
        /* If the bridge/server disappears while ForumCE is open,
           Home returns here and the same startup connection screen
           becomes the reconnect gate. No UI redesign is required. */
    }

    network_cleanup();
    gfx_End();

    return 0;
}
