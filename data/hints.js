var elements = [];
var active_arr = [];
var hints;
var overlays;
var active;
var last_active;
var lastpos = 0;
var lastinput;
var styles;
var form_hints = "//form";
var last_entry = null;

function Hint(element) {
  this.element = element;
  this.rect = element.getBoundingClientRect();

  function create_span(element) {
    var span = document.createElement("span");
    var leftpos = Math.max((element.rect.left + document.defaultView.scrollX), document.defaultView.scrollX) ;
    var toppos = Math.max((element.rect.top + document.defaultView.scrollY), document.defaultView.scrollY) ;
    span.style.position = "absolute";
    span.style.left = leftpos + "px";
    span.style.top = toppos + "px";
    return span;
  }
  function create_hint(element) {
    var hint = create_span(element);
    hint.style.fontSize = hint_font_size;
    hint.style.fontWeight = hint_font_weight;
    hint.style.fontFamily = hint_font_family;
    hint.style.color = hint_fg_color;
    hint.style.background = hint_bg_color;
    hint.style.opacity = hint_opacity;
    hint.style.border = hint_border;
    hint.style.zIndex = 20000;
    hint.style.visibility = 'visible';
    return hint;
  }

  this.hint = create_hint(this);
}
//NumberHint
NumberHint.prototype.getTextHint = function (i, length) {
  start = length <=10 ? 1 : length <= 100 ? 10 : 100;
  var content = document.createTextNode(start + i);
  this.hint.appendChild(content);
}
NumberHint.prototype.betterMatch = function(input) {
  var bestposition = 37;
  var ret = 0;
  for (var i=0; i<active_arr.length; i++) {
    var e = active_arr[i];
    if (input && bestposition != 0) {
      var content = e.element.textContent.toLowerCase().split(" ");
      for (var cl=0; cl<content.length; cl++) {
        if (content[cl].toLowerCase().indexOf(input) == 0) {
          if (cl < bestposition) {
            ret = i;
            bestposition = cl;
            break;
          }
        }
      }
    }
  }
  return ret;
}
NumberHint.prototype.matchText = function(input) {
  var ret = false;
  if (parseInt(input) == input) {
    text_content = this.hint.textContent;
  }
  else {
    text_content = this.element.textContent.toLowerCase();
  }
  if (text_content.match(input)) {
    return true;
  }
}

// LetterHint
LetterHint.prototype.getTextHint = function(i, length) {
  var text;
  var l = hint_letter_seq.length;
  if (length < l) {
    text = hint_letter_seq[i];
  }
  else if (length < 2*l) {
    var rem = (length) % l;
    var sqrt = Math.sqrt(2*rem);
    var r = sqrt == (getint = parseInt(sqrt)) ? sqrt + 1 : getint ;
    if (i < l-r) {
      text = hint_letter_seq[i];
    }
    else {
      var newrem = i%(r*r);
      text = hint_letter_seq[Math.floor( (newrem / r) + l - r )] + hint_letter_seq[l-newrem%r - 1];
    }
  }
  else {
    text = hint_letter_seq[i%l] + hint_letter_seq[l - 1 - (parseInt(i/l))];
  }
  var content = document.createTextNode(text);
  this.hint.appendChild(content);
}

LetterHint.prototype.betterMatch = function(input) {
  return 0;
}

LetterHint.prototype.matchText = function(input) {
  return (this.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) == 0);
}


function LetterHint(element) {
  this.constructor = Hint;
  this.constructor(element);
}
LetterHint.prototype = new Hint();

function NumberHint(element) {
  this.constructor = Hint;
  this.constructor(element);
}

NumberHint.prototype = new Hint();

function click_element(e) {
  var mouseEvent = document.createEvent("MouseEvent");
  mouseEvent.initMouseEvent("click", true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
  e.element.dispatchEvent(mouseEvent);
  clear();
}
function create_stylesheet() {
  var styles = document.createElement("style");
  styles.type = "text/css";
  document.getElementsByTagName('head')[0].appendChild(styles);
  
  var style = document.styleSheets[document.styleSheets.length - 1];
  style.insertRule('a[dwb_highlight=hint_normal] { background: ' + hint_normal_color + ' } ', 0);
  style.insertRule('a[dwb_highlight=hint_normal] { outline: 1p solid ' + hint_normal_color + ' } ', 0);
  style.insertRule('input[dwb_highlight=hint_normal] { outline: 1px solid ' + hint_normal_color + '  } ', 0);
  style.insertRule('a[dwb_highlight=hint_active] { background: ' + hint_active_color + '  } ', 0);
  style.insertRule('a[dwb_highlight=hint_active] { border: 2px solid ' + hint_active_color + '  } ', 0);
  style.insertRule('input[dwb_highlight=hint_active] { outline: 2px solid ' + hint_active_color + '  } ', 0);
}

function get_visibility(e) {
  var rects = e.getClientRects()[0];
  var r = e.getBoundingClientRect();
  if (!r || r.top > window.innerHeight || r.bottom < 0 
      || r.left > window.innerWidth ||  r < 0 || !rects ) {
    return false;
  }
  var style = document.defaultView.getComputedStyle(e, null);
    if (style.getPropertyValue("visibility") != "visible" 
        || style.getPropertyValue("display") == "none") {
      return false;
    }
    return true;
}


function show_hints(w) {
  if (!w) {
    w = window;
  }
  var doc = w.document;
  document.activeElement.blur();
  var res = doc.body.querySelectorAll('a, area, textarea, select, link, input:not([type=hidden]), button,  frame, iframe');
  hints = document.createElement("div");
  create_stylesheet();
  for (var i=0; i<res.length; i++) {
    if (get_visibility(res[i])) {
      var e = hint_style.toLowerCase() == "letter" ? new LetterHint(res[i]) : new NumberHint(res[i]);
      hints.appendChild(e.hint);
      elements.push(e);
    }
  };
  elements.sort( function(a,b) { return a.rect.top - b.rect.top; });
  for (var i=0; i<elements.length; i++) {
    var e = elements[i];
    e.getTextHint(i, elements.length);
    e.element.setAttribute('dwb_highlight', 'hint_normal');
  }
  active_arr = elements;
  
  var  frames = w.frames;
  for (var i=0; i<frames.length; i++) {
    show_hints(frames[i]);
  }
  document.body.appendChild(hints);
}

function is_input(element) {
  var e = element.element;
  var type = e.type.toLowerCase();
  var  tagname = e.toLowerCase();
  if (tagname == "input" || tagname == "textarea" ) {
    if (type == "radio" || type == "checkbox") {
      e.checked = !e.checked;
      return false;
    }
    else if (type == "submit" || type == "reset" || type  == "button") {
      click_element(element);
    }
    else {
      e.focus();
    }
    return true;
  }
  return false;
}

function update_hints(input) {
  var array = [];
  var text_content;
  var keep = false;
  if (input) {
    input = input.toLowerCase();
  }
  if (lastinput && (lastinput.length > input.length)) {
    clear();
    lastinput = input;
    show_hints();
    update_hints(input);
    return;
  }
  lastinput = input;
  for (var i=0; i<active_arr.length; i++) {
    var e = active_arr[i];
    if (e.matchText(input)) {
      array.push(e);
    }
    else {
      e.hint.style.visibility = 'hidden';
      e.element.removeAttribute('dwb_highlight');
    }
  }
  active_arr = array;
  active = array[0];
  if (array.length == 0) {
    clear();
    return "_dwb_no_hints_";
  }
  else if (array.length == 1) {
    return  evaluate(array[0]);
  }
  else {
    lastpos = array[0].betterMatch(input);
    set_active(array[lastpos]);
  }
  last_entry = null;
}
function set_active(element) {
  var active = document.querySelector('*[dwb_highlight=hint_active]');
  if (active) {
    active.setAttribute('dwb_highlight', 'hint_normal' );
  }
  element.element.setAttribute('dwb_highlight', 'hint_active');
  active = element;
}
function clear() {
  if (elements) {
    for (var i=0; i<elements.length; i++) {
      elements[i].element.removeAttribute('dwb_highlight');
    }
  }
  if (styles) {
    document.getElementsByTagName('head')[0].removeChild(styles);
  }
  if (hints && hints.parentNode) {
    hints.parentNode.removeChild(hints);
    hints = undefined;
  }
  if (overlays && overlays.parentNode) {
    overlays.parentNode.removeChild(overlays);
    overlays = undefined;
  }
  elements = [];
  active_arr = [];
  //active = undefined;
}

function evaluate(element) {
  var ret;
  var e = element.element;
  var type = e.type.toLowerCase();
  var  tagname = e.tagName.toLowerCase();

  if (tagname == "input" || tagname == "textarea" ) {
    if (type == "radio" || type == "checkbox") {
      e.focus();
      click_element(element);
      ret = "_dwb_check_";
    }
    else if (type == "submit" || type == "reset" || type  == "button") {
      click_element(element);
      ret = "_dwb_click_";
    }
    else {
      e.focus();
      ret = "_dwb_input_";
    }
  }
  else if (e.href) {
    if (e.href.match(/javascript:/) || (e.type.toLowerCase() == "button")) {
      click_element(element);
      ret = "_dwb_click_";
    }
    else {
      ret = e.href;
    }
  }
  clear(); 
  return ret;
}
function get_active() {
  return evaluate(active);
}

function focus_next() {
  var newpos = lastpos == active_arr.length-1 ? 0 : lastpos + 1;
  active = active_arr[newpos];
  set_active(active);
  lastpos = newpos;
}
function focus_prev() {
  var newpos = lastpos == 0 ? active_arr.length-1 : lastpos - 1;
  active = active_arr[newpos];
  set_active(active);
  lastpos = newpos;
}
function add_searchengine() {
  clear();
  create_stylesheet();
  var hints = document.createElement("div");
  var res = document.body.querySelectorAll("form");

  for (var i=0; i<res.length; i++) {
    var els = res[i].elements;
    for (var j=0; j<els.length; j++) {
      if (get_visibility(els[j]) && els[j].type == "text") {
        var e = new LetterHint(els[j]);
        elements.push(e);
        e.element.setAttribute('dwb_highlight', 'hint_normal');
      }
    }
  }
  if (!elements.length) {
    return "_dwb_no_hints_";
  }
  elements.sort( function(a,b) { return a.rect.top - b.rect.top; });
  for (var i=0; i<=elements; i++) {
    elements[i].getTextHint(i, elements.length);
    hints.appendChild(elements[i].hint);
  }
  document.body.appendChild(hints); 
  set_active[elements[0]];
  active_arr = elements;
  //update_hints();
}
function submit_searchengine(string) {
  var e = active.element;
  e.value = string;
  e.form.submit();
  e.value = "";
  if (e.form.method.toLowerCase() == 'post') {
    return e.name;
  }
  return NULL;
}

var active_input;
function focus_input() {
  var res = document.body.querySelectorAll('input[type=text], input[type=password], textarea');
  if (res.length == 0) {
    return "_dwb_no_input_";
  }
  styles = document.styleSheets[0];
  styles.insertRule('input:focus { outline: 2px solid #1793d1; }', 0);
  if (!active_input) {
    active_input = res[0];
  }
  else {
    for (var i=0; i<res.length; i++) {
      if (res[i] == active_input) {
        if (!(active_input = res[i+1])) {
          active_input = res[0];
        }
        break;
      }
    }
  }
  active_input.focus();
}
function input_get_name() {
  //create_stylesheet();
  var e = document.activeElement;
  if (e.tagName.toLowerCase() == "input") {
    last_entry = e;

    e.focus();
    if (last_entry) {
      return document.activeElement.type;
    }
  }
}
function input_set_text(text) {
  if (last_entry) {
    last_entry.value = text;
    last_entry = null;
  }
}
