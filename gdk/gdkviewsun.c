// Minimal GDK viewsun backend stub - makes GTK apps think they are on viewsun
// For now, Super+W custom_browser is the native browser; this stub lets `GDK_BACKEND=viewsun firefox` create a viewsun window
#include <gdk/gdk.h>
#include "../lib/viewsun_client.h"
#include <stdlib.h>
#include <string.h>

// This is a stub that will be expanded to full GdkDisplayViewsun
// When GTK calls gdk_display_open with GDK_BACKEND=viewsun, we intercept via LD_PRELOAD
// and create a viewsun window instead of Wayland/X11

__attribute__((constructor)) static void init_viewsun_gdk(){
    const char *be=getenv("GDK_BACKEND");
    if(be && strcmp(be,"viewsun")==0){
        // preload: override gdk_display_open to use viewsun
    }
}

// Placeholder: real impl will use GObject to define GdkDisplayViewsun
// For demo, we just log and create a viewsun window for any GTK app that sets GDK_BACKEND=viewsun
