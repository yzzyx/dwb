#!/bin/bash

INFILE=$1
OUTFILE=$2
[ -f ${OUTFILE} ] && rm ${OUTFILE}
i=0
while read; do 
  array=( $REPLY )
  if [ "${array[0]}" = "#" ]; then 
    echo "<tr class='dwb_table_row'><th class='dwb_table_headline' colspan='3'>${array[@]:2}</th></tr>" >> ${OUTFILE}
  elif [[ ! ${REPLY} =~ ^\ *$ ]]; then 
    cat >> ${OUTFILE} <<EOF 
  <tr class='dwb_table_row${i}'>
    <td class='dwb_table_cell_left'>${array[0]}</td>
    <td class='dwb_table_cell_middle'>${array[@]:1}</td>
    <td class='dwb_table_cell_right'> <input id='${array[0]}' type='text' > </td>
  </tr>
EOF
    ((i++))
    ((i%=2))
  fi
done < $INFILE
