$1 == "#" {
	$1 = ""; $2 = ""
	sub(/^[[:space:]]*/, "")
	print "<tr class='dwb_table_row'><th class='dwb_table_headline' colspan='3'>" $0 "</th></tr>"
	next
}

NF > 0 {
	id = $1; $1 = ""
	sub(/^[[:space:]]*/, "")
  if (NR % 2 == 0) {
    print "<tr class='dwb_table_row_even'>"
  }
  else {
    print "<tr class='dwb_table_row_odd'>"
  }
	print "  <td class='dwb_table_cell_left'>" id "</td>"
	print "  <td class='dwb_table_cell_middle'>" $0 "</td>"
	print "  <td class='dwb_table_cell_right'> <input id='" id "' type='text' > </td>"
	print "</tr>"
}
END {
  print "<tr class='dwb_table_row_even'><th class='dwb_table_headline' colspan='3'>Custom commands</th></tr>"
	print "<td colspan='3'><div class='desc'>Custom commands can be defined in the following form:"
  print "</div><div class='commandLineContainer'><div class='commandline'>[shortcut]:[command];;[command];;...</div></div>"
	print "</td></tr>"
  print "<tr class='dwb_table_row'><td colspan='3'><textarea rows='10' id='dwb_custom_keys_area'></textarea><td></tr>"
	print "<tr class='dwb_table_row'>"
	print "<tr class='dwb_table_row'>"
	print "  <td class='dwb_table_cell_right' colspan='3'><input id='dwb_custom_keys_submit' type='button' value='save custom'></input></td>"
	print "</tr>"
}
