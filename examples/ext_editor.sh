#!/bin/bash
# dwb: Control e

FILE=/tmp/dwb_edit

xterm -e vim ${FILE}

CONTENT+=$(sed 's/^\(.*\)$/value+=\"\1\\n\";/' ${FILE})

echo "open javascript:var value='';var a=document.activeElement;${CONTENT}a.value=value;" | tr -d '\n'

#vim tw=0
