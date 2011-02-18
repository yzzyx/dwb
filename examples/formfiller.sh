#!/bin/bash 
# dwb: control mod1 f

# This script can be used to automatically fill out forms.  The data must be
# saved in a different file (CONTENT_FILE) each line specifies a form and must
# be of the form
# <url> <element_1_name> <element_1_value> <element_2_name> <element_2_value> ... 
# example: https://bbs.archlinux.org/login.php req_username username req_password password save_pass true

CONTENT_FILE=${XDG_CONFIG_HOME}/dwb/data/forms
SUBMIT=1 

COMMAND+="javascript:function set_value(e, value){if(e.type==\"checkbox\"||e.type==\"radio\"){e.checked=(value.toLowerCase()!=='false'&&value!=='0');}else{e.value=value;}}"

while read; do
  read -a LINE -d $'\0' <<<"${REPLY}"
  if [[ ${LINE[0]} = $1 ]]; then 
    for ((i=1; i<${#LINE[@]}; i++)); do
      NAME=${LINE[$((i++))]}
      VALUE=${LINE[$((i))]}
      COMMAND+="var ${NAME}=document.getElementsByName(\"${NAME}\")[0];set_value(${NAME},\"${VALUE}\");"
    done
    test ${SUBMIT} != 0 && COMMAND+="${NAME}.form.submit();"
  fi
done < "${CONTENT_FILE}"
printf "open ${COMMAND}\nclose\n"

# vim: tw=0
