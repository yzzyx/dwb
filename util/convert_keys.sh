#!/bin/bash


declare -A map

INPUT="$XDG_CONFIG_HOME/dwb/keys"
[ "$1" ] && INPUT="$1"


map=(
[open_url]=Open 
[tabopen]=open_nv 
[tabopen_url]=Open_nv 
[winopen]=open_nw 
[winopen_url]=Open_nw 
[tab_hist_back]=history_back_nv
[tab_hist_forward]=history_forward_nv
[win_hist_back]=history_back_nw 
[win_hist_forward]=history_forward_nw
[hints]=hint_mode
[hints_tab]=hint_mode_nv
[hints_win]=hint_mode_nw
[hints_links]=hint_mode_links
[hints_images]=hint_mode_images
[hints_images_tab]=hint_mode_images_nv
[hints_editable]=hint_mode_editable
[hints_url]=hint_mode_url
[hints_url_tab]=hint_mode_url_nv
[hints_download]=hint_mode_download
[hints_clipboard]=hint_mode_clipboard
[hints_primary]=hint_mode_primary
[hints_rapid]=hint_mode_rapid
[hints_rapid_win]=hint_mode_rapid_nw
[tab_bookmarks]=bookmarks_nv
[win_bookmarks]=bookmarks_nw
[quickmark]=open_quickmark
[tab_quickmark]=open_quickmark_nv
[win_quickmark]=open_quickmark_nw
[start_page]=open_start_page
[tab_new]=add_view
[close_tab]=remove_view
[focus_tab]=focus_nth_view
[tab_paste]=past_nv
[win_paste]=paste_nw
[tab_paste_primary]=paste_primary_nw
[win_paste_primary]=paste_primary_nv
[new_tab]=new_view
[new_win]=new_window
)

for key in ${!map[@]}; do 
  sed -i "s/${map[$key]}/$key/" "${INPUT}"
done
