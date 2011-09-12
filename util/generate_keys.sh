#!/bin/bash

INFILE=keys.in
OUTFILE=keys.out
cat > ${OUTFILE} << EOF
<div class='setting_bar'>
  <a class="setting_button" href="#general">General</a>
  <a class="setting_button" href="#bookmark_keys">Book- &amp Quickmarks</a>
  <a class="setting_button" href="#commandline">Commandline</a>
  <a class="setting_button" href="#scrolling">Scrolling</a>
  <a class="setting_button" href="#tabs">Tabs &amp UI</a>
  <a class="setting_button" href="#clipboard">Clipboard</a>
  <a class="setting_button" href="#settings">Settings</a>
  <a class="setting_button" href="#misc">Miscellaneous</a>
  </div>
<div class='dwb_settings_panel'>
EOF

i=0
while read; do 
  array=( $REPLY )
  if [ "${array[0]}" = "#" ]; then 
    echo "<div class='dwb_settings_headline'><a name='${array[1]}'>${array[@]:2}</a></div>" >> ${OUTFILE}
  else 
    cat >> ${OUTFILE} <<EOF 
  <div class='dwb_line${i}'>
    <div class='dwb_attr'>${array[0]}</div>
    <div style='float:right;'>
      <label class='dwb_desc' for='${array[0]}'>${array[@]:1}</label>
      <input id='${array[0]}' type='text' >
    </div>
    <div style='clear:both;'></div>
  </div>
EOF
  ((i++))
  ((i%=2))
  fi
done < $INFILE
echo "</div>" >> ${OUTFILE}
