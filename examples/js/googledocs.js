#!javascript

/*
 * Shortcut to toggle usage of Google Docs
 */
var shortCut = "tgd";

/*
 * Whether to initially use Google Docs
 */
var useDocs = true;

/*
 * list off all supported filetypes, comment / uncomment to enable/disable
 * filetype
 */
var supported = [ 
  "DOC", 
  "DOCX", 
  "XLS", 
  "XLSX", 
  "PPT", 
  "PPTX", 
  "ODT", 
  "ODS",
  "PDF", 
  "PAGES", 
  "AI", 
  "PSD", 
  "TIFF", 
  "DXF", 
  "SVG", 
  "EPS", 
  "PS", 
  "TTF",
  "OTF", 
  "XPS", 
  // "ZIP", 
  // "RAR"  
];


var id = signals.connect("download", downloadCheck);
var reg = new RegExp(".*\.(" + supported.join("|") + ")$", "i");

function downloadCheck(w, d) {
  if (w.mainFrame.host === "docs.google.com")
    return false;
  else if (reg.test(d.networkRequest.uri)) {
    w.loadUri("http://docs.google.com/viewer?url=" + d.networkRequest.uri);
    return true;
  }
}
function toggleDocs() {
  useDocs = !useDocs;
  if (!useDocs) {
    signals.disconnect(id);
    io.notify("Google Docs disabled");
  }
  else {
    id = signals.connect("download", downloadCheck);
    io.notify("Google Docs enabled");
  }
}
bind(shortCut, toggleDocs, "googleDocs");
