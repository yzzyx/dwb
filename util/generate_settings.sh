#!/bin/bash 

INFILE=$1
OUTFILE=$2
cat > ${OUTFILE} << EOF
<div class='setting_bar'>
  <a class="setting_button" href="#general">General</a>
  <a class="setting_button" href="#session">Network &amp Session</a>
  <a class="setting_button" href="#fonts">Fonts</a>
  <a class="setting_button" href="#colors">Colors</a>
  <a class="setting_button" href="#lt">Layout</a>
  <a class="setting_button" href="#hints">Hints</a>
  <a class="setting_button" href="#plugins">Plugins &amp Scripts</a>
  <a class="setting_button" href="#completion">Completion</a>
  <a class="setting_button" href="#misc">Miscellaneous</a>
</div>
<div class='dwb_settings_panel'>
EOF
i=0
while read; do 
  line=( ${REPLY} )
  if [ "${line[0]}" = "#" ]; then 
    echo "<div class='dwb_settings_headline'><a name='${line[1]}'>${line[@]:2}</a></div>" >> ${OUTFILE}
  elif [[ ! ${REPLY} =~ ^\ *$ ]]; then 
    if [ ${line[1]} = "select" ]; then 
      unset options;
      unset desc;
      for ((i=2; i<${#line[@]}; i++)); do 
        if [[ ${line[i]} =~ ^@ ]]; then 
          option=${line[i]//_/ }
          options+="<option>${option:1}</option>"
        else 
          desc=${line[@]:i}
          break
        fi
      done
      cat >> ${OUTFILE} << EOF
  <div class='dwb_line$i'>
    <div class='dwb_attr'>${line[0]}</div>
      <div style='float:right;'>
        <label for='${line[0]}' class='dwb_desc'>${desc}</label>
        <select id='${line[0]}'>
          ${options}
        </select>
      </div>
    <div style='clear:both;'></div>
  </div>
EOF
    else 
      cat >> ${OUTFILE} << EOF
  <div class='dwb_line$i'>
    <div class='dwb_attr'>${line[0]}</div>
      <div style='float:right;'>
        <label for='${line[0]}' class='dwb_desc'>${line[@]:2}</label>
        <input id='${line[0]}' type='${line[1]}'> 
    </div>
    <div style='clear:both;'></div>
  </div>
EOF
    fi
    ((i++))
    ((i%=2))
  fi
done < ${INFILE}
echo "</div>" >> ${OUTFILE}
