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

# log for debugging DRM master crashes
exec >"$HOME/.viewsun.log" 2>&1
echo "viewsun-login: $(date) WALLPAPER=$WALLPAPER card=${VIEWSUN_CARD:-auto} DISPLAY=$DISPLAY XDG_SESSION_TYPE=$XDG_SESSION_TYPE"
exec /usr/local/bin/viewsun -w "$WALLPAPER" --backend auto
