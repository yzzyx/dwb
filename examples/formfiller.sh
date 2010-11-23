#!/bin/bash
# dwb: Control Mod1 f

# This script can be used to automatically fill out forms.  The data must be
# saved in a different file (CONTENT_FILE); each line specifies a form and must
# be of the form
# <url> <element_1_name> <element_1_value> <element_2_name> <element_2_name> ... 
# example: https://bbs.archlinux.org/login.php req_username username req_password password save_pass true

CONTENT_FILE=${XDG_CONFIG_HOME}/dwb/data/forms

echo "js \
  function set_value(e, value) { \
    if (e.type == \"checkbox\" || e.type == \"radio\") { \
      e.checked = (value.toLowerCase() !== 'false' && value !== '0'); \
    } \
    else  \
      e.value = value; \
  }"

while read; do
  read -a LINE -d $'\0' <<<"${REPLY}"
  if [[ ${LINE[0]} = $1 ]]; then 
    for ((i=1; i<${#LINE[@]}; i++)); do
      NAME=${LINE[$((i++))]}
      VALUE=${LINE[$((i))]}
      echo "js var ${NAME} = document.getElementsByName(\"${NAME}\")[0]; set_value(${NAME}, \"${VALUE}\");"
    done
    echo "js var f = ${NAME}.form; f.submit();"
    exit 0
  fi
done < ${CONTENT_FILE}

# vim: tw=0
