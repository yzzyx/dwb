#!/bin/bash
# dwb: Control e

FILE=/tmp/dwb_edit

xterm -e vim ${FILE}

CONTENT=$(sed 's/^\(.*\)$/js a.value += \"\1\\n\"/' ${FILE})

cat << EOF
js var a = document.activeElement;
${CONTENT}
EOF
