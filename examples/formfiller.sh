#!/bin/bash 
# dwb: control mod1 f
 
# This script can be used to automatically fill out forms.  The data must be
# saved in a different file (CONTENT_FILE) each line specifies a form and must
# be of the form
# <url> <submit> <element_1_name> <element_1_value> <element_2_name> <element_2_value> ... 
# example: https://bbs.archlinux.org/login.php true req_username username req_password password save_pass true
 
CONTENT_FILE=${XDG_CONFIG_HOME}/dwb/data/forms
 
COMMAND+="exja function set_value(e, value){if(e.type==\"checkbox\"||e.type==\"radio\"){e.checked=(value.toLowerCase()!=='false'&&value!=='0');}else{e.value=value;}}"
 
while read; do
  read -a LINE -d $'\0' <<<"${REPLY}"
  if [[ "$DWB_URI" =~ ${LINE[0]} ]]; then 
    if [[ ${LINE[1]} = "true" ]]; then
      SUBMIT=1
    else
      SUBMIT=0
    fi
    for ((i=2; i<${#LINE[@]}; i++)); do
      NAME=${LINE[$((i++))]}
      VALUE=${LINE[$((i))]}
      COMMAND+="var ${NAME}=document.getElementsByName(\"${NAME}\")[0];set_value(${NAME},\"${VALUE}\");"
    done
    test ${SUBMIT} != 0 && COMMAND+="${NAME}.form.submit();"
  fi
done < "${CONTENT_FILE}"
printf "${COMMAND}\nclose\n" 
 
# vim: tw=0
