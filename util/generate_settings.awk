$1 == "#" {
	$1 = ""; $2 = "";
	sub(/^[[:space:]]*/, "");
  print "<tr class='dwb_table_row'>"
	print "  <th class='dwb_table_headline' colspan='3'>" $0 "</th>"
	print "</tr>"
	next
}

$2 == "select" {
	id = $1; $1 = ""; $2 = ""; options = ""
	for (i = 3; $i ~ /^@/ && i <= NF; ++i) {
    value = substr($i, 2)
		gsub(/_/, " ", value)
		options = options "<option>" value "</option>"
		$i = ""
	}
	sub(/^[[:space:]]*/, "")
  if (NR % 2 == 0) {
    print "<tr class='dwb_table_row_even'>"
  }
  else {
    print "<tr class='dwb_table_row_odd'>"
  }
	print "  <td class='dwb_table_cell_left'>" id "</td>"
	print "  <td class='dwb_table_cell_middle'>" $0 "</td>"
	print "  <td class='dwb_table_cell_right'>"
        print "    <select id='" id "'>"
	print "      " options
        print "    </select>"
	print "  </td>"
	print "</tr>"
	next
}

NF > 0 {
	id = $1; type = $2; $1 = ""; $2 = ""
	sub(/^[[:space:]]*/, "")
  if (NR % 2 == 0) {
    print "<tr class='dwb_table_row_even'>"
  }
  else {
    print "<tr class='dwb_table_row_odd'>"
  }
	print "  <td class='dwb_table_cell_left'>" id "</td>"
	print "  <td class='dwb_table_cell_middle'>" $0 "</td>"
	print "  <td class='dwb_table_cell_right'><input id='" id "' type='" type "'></td>"
	print "</tr>"
}
