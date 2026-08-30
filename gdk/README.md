# Viewsun GDK Backend (GTK → Viewsun)

Native GTK backend so `firefox`, `nautilus` etc run directly on Viewsun custom compositor without Wayland/X.

- `GDK_BACKEND=viewsun` → `gdk_display_open` → `viewsun_create_window` via `/tmp/viewsun-0`
- Cairo draws into SHM `0xFFRRGGBB` true RGB, `damage` → compositor blit
- Input: Viewsun `evdev` → `GdkEventKey/Button/Motion`

Status: stub, custom_browser is the reference impl. Full Firefox via GDK coming next.

Build: `make gdk`
