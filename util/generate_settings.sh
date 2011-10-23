#!/bin/bash 

INFILE=$1
OUTFILE=$2
[ -f ${OUTFILE} ] && rm ${OUTFILE}
while read; do 
  line=( ${REPLY} )
  if [ "${line[0]}" = "#" ]; then 
    cat >> ${OUTFILE} <<EOF 
    <tr class='dwb_table_row'>
      <th class='dwb_table_headline' colspan='3'>${line[@]:2}</th>
    </tr>
EOF
  elif [[ ! ${REPLY} =~ ^\ *$ ]]; then 
    if [ ${line[1]} = "select" ]; then 
      unset options;
      unset desc;
      for ((j=2; j<${#line[@]}; j++)); do 
        if [[ ${line[j]} =~ ^@ ]]; then 
          option=${line[j]//_/ }
          options+="<option>${option:1}</option>"
        else 
          desc=${line[@]:j}
          break
        fi
      done
      cat >> ${OUTFILE} << EOF
  <tr class='dwb_table_row'>
    <td class='dwb_table_cell_left'>${line[0]}</td>
    <td class='dwb_table_cell_middle'>${desc}</td>
    <td class='dwb_table_cell_right'>
        <select id='${line[0]}'>
          ${options}
        </select>
    </td>
  </tr>
EOF
    else 
      cat >> ${OUTFILE} << EOF
  <tr class='dwb_table_row'>
    <td class='dwb_table_cell_left'>${line[0]}</td>
    <td class='dwb_table_cell_middle'>${line[@]:2}</td>
    <td class='dwb_table_cell_right'><input id='${line[0]}' type='${line[1]}'></td>
  </tr>
EOF
    fi
  fi
done < ${INFILE}
