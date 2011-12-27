$1 == "#" {
	$1 = ""; $2 = ""
	sub(/^[[:space:]]*/, "")
	print "<tr class='dwb_table_row'><th class='dwb_table_headline' colspan='3'>" $0 "</th></tr>"
	next
}

NF > 0 {
	id = $1; $1 = ""
	sub(/^[[:space:]]*/, "")
	print "<tr class='dwb_table_row'>"
	print "  <td class='dwb_table_cell_left'>" id "</td>"
	print "  <td class='dwb_table_cell_middle'>" $0 "</td>"
	print "  <td class='dwb_table_cell_right'> <input id='" id "' type='text' > </td>"
	print "</tr>"
}