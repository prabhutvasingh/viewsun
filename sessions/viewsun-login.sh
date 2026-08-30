#!/bin/sh
# Viewsun login wrapper - selects wallpaper and launches compositor
# Called by display manager (GDM/SDDM/LightDM) or via ~/.xinitrc

WALLPAPER="$HOME/.config/viewsun/wallpaper.png"
[ -n "$1" ] && WALLPAPER="$1"

# try user wallpaper, fallback to system
if [ ! -f "$WALLPAPER" ]; then
  for p in "$HOME/Pictures/wallpaper.jpg" "$HOME/wallpaper.png" "/usr/share/viewsun/wallpaper.png" "/usr/share/backgrounds/default.png"; do
    [ -f "$p" ] && WALLPAPER="$p" && break
  done
fi

# ensure viewSun config dir
mkdir -p "$HOME/.config/viewsun"

exec /usr/local/bin/viewsun -w "$WALLPAPER" --backend auto
