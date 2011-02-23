var hint_letter_seq, hint_font_size, hint_font_weight, hint_font_family,
    hint_style, hint_fg_color, hint_bg_color, hint_active_color,
    hint_normal_color, hint_border, hint_opacity;
var elements = [];
var active_arr = [];
var hints;
var overlays;
var overlays;
var active;
var last_active;
var lastpos = 0;
var lastinput;
var styles;
var form_hints = "//form";
var hint_types = 'a, img, textarea, select, link, input:not([type=hidden]), button,  frame, iframe, *[onclick], *[onmousedown], *[role=link]';

var styles = null;

function DwbHint(element, win, offset) {
  this.element = element;
  this.win = win;
  if (!offset) {
    offset = [ 0, 0 ];
  }

  function create_span(element) {
    var span = document.createElement("span");
    var rect;
    var leftpos, toppos;
    if (element instanceof HTMLAreaElement) {
      var coords = element.coords.split(",");
      var leftpos = offset[0] + document.defaultView.scrollX + parseInt(coords[0]);
      var toppos = offset[1] + document.defaultView.scrollY + parseInt(coords[1]);
    }
    else {
      rect = element.getBoundingClientRect();
      leftpos = offset[0] + Math.max((rect.left + document.defaultView.scrollX), document.defaultView.scrollX) ;
      toppos = offset[1] + Math.max((rect.top + document.defaultView.scrollY), document.defaultView.scrollY) ;
    }
    span.style.position = "absolute";
    span.style.left = leftpos  + "px";
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
    hint.name = "dwb_hint";
    return hint;
  }

  this.hint = create_hint(element);
}
//DwbNumberHint
DwbNumberHint.prototype.getTextHint = function (i, length) {
  start = length <=10 ? 1 : length <= 100 ? 10 : 100;
  var content = document.createTextNode(start + i);
  this.hint.appendChild(content);
}
DwbNumberHint.prototype.betterMatch = function(input) {
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
DwbNumberHint.prototype.matchText = function(input) {
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

// DwbLetterHint
DwbLetterHint.prototype.getTextHint = function(i, length) {
  var text;
  var l = hint_letter_seq.length;
  if (length < l) {
    text = hint_letter_seq[i];
  }
  else if (length < 2*l) {
    var rem = (length) % l;
    var sqrt = Math.sqrt(2*rem);
    var r = sqrt == (getint = parseInt(sqrt)) ? sqrt + 1 : getint;
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

DwbLetterHint.prototype.betterMatch = function(input) {
  return 0;
}

DwbLetterHint.prototype.matchText = function(input) {
  return (this.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) == 0);
}


function DwbLetterHint(element, win, offset) {
  this.constructor = DwbHint;
  this.constructor(element, win, offset);
}

function DwbNumberHint(element, win, offset) {
  this.constructor = DwbHint;
  this.constructor(element, win, offset);
}

function dwb_mouse_event(element, ev) {
  var e = element.element;
  var mouseEvent = document.createEvent("MouseEvent");
  mouseEvent.initMouseEvent(ev, true, true, element.win, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
  e.dispatchEvent(mouseEvent);
}
function dwb_click_element(element, ev) {
  if (ev) {
    dwb_mouse_event(element, ev);
  }
  else { // both events, if no event is given
    dwb_mouse_event(element, "click");
    dwb_mouse_event(element, "mousedown");
  }
  dwb_clear();
}
function dwb_hex_to_rgb(color) {
  var rgb;
  if (color[0] !== '#')
    return color;
  if (color.length == 4) {
    rgb = /#([0-9a-f])([0-9a-f])([0-9a-f])/i.exec(color);
    for (var i=1; i<=3; i++) {
      var v = parseInt("0x" + rgb[i])+1;
      rgb[i] = v*v-1;
    }
  }
  else {
    rgb  = /#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})/i.exec(color);
    for (var i=1; i<=3; i++) {
      rgb[i] = parseInt("0x" + rgb[i]);
    }
  }
  return "rgba(" + rgb.slice(1) + "," +  hint_opacity + ")";
}
function dwb_create_stylesheet() {
  if (styles)
    return;
  styles = document.createElement("style");
  styles.type = "text/css";
  document.head.appendChild(styles);

  var style = document.styleSheets[document.styleSheets.length - 1];

  style.insertRule('*[dwb_highlight=hint_normal] { background: ' + dwb_hex_to_rgb(hint_normal_color) + ' !important; } ', 0);
  style.insertRule('*[dwb_highlight=hint_active] { background: ' + dwb_hex_to_rgb(hint_active_color) + ' !important; } ', 0);
}

function dwb_get_visibility(e) {
  var style = document.defaultView.getComputedStyle(e, null);
  if ((style.getPropertyValue("visibility") == "hidden" || style.getPropertyValue("display") == "none" ) ) {
      return false;
  }
  var rects = e.getClientRects()[0];
  var r = e.getBoundingClientRect();

  var height = window.innerHeight ? window.innerHeight : document.body.offsetHeight;
  var width = window.innerWidth ? window.innerWidth : document.body.offsetWidth;
  if (!r || r.top > height || r.bottom < 0 || r.left > width ||  r.right < 0 || !rects) {
    return false;
  }
  return true;
}

function dwb_get_element(win, e, offset, constructor) {
  var leftoff = 0;
  var topoff = 0;
  if (offset) {
    leftoff += offset[0];
    topoff += offset[1];
  }
  if ( (e instanceof HTMLIFrameElement || e instanceof HTMLFrameElement) && e.contentDocument) {
    var res = e.contentDocument.body.querySelectorAll(hint_types);
    var off = [ leftoff + e.offsetLeft, topoff + e.offsetTop ];
    for (var i=0; i < res.length; i++) {
      dwb_get_element(e, res[i], off, constructor);
    }
  }
  else if (e instanceof HTMLImageElement) {
    if (!e.useMap) 
      return;
    var areas = e.parentNode.getElementsByTagName("area");
    var r = e.getBoundingClientRect();
    for (var i=0; i<areas.length; i++) {
      var element = new constructor(areas[i], win, [leftoff + r.left, topoff + r.top]);
      elements.push(element);
    }
  }
  else {
    if (dwb_get_visibility(e)) {
      var off = [ leftoff, topoff ];
      var element = new constructor(e, win, off);
      elements.push(element);
    }
  }
}

function dwb_show_hints() {
  document.activeElement.blur();

  var hints = document.createElement("div");
  hints.id = "dwb_hints";
  var constructor = hint_style.toLowerCase() == "letter" ? DwbLetterHint : DwbNumberHint;

  dwb_create_stylesheet();

  var res = document.body.querySelectorAll(hint_types);
  for (var i=0; i<res.length; i++) {
    dwb_get_element(window, res[i], null, constructor);
  };
  for (var i=0; i<elements.length; i++) {
    if (res[i] == elements[i]) {
      continue;
    }
    var e = elements[i];
    hints.appendChild(e.hint);
    e.getTextHint(i, elements.length);
    e.element.setAttribute('dwb_highlight', 'hint_normal');
  }
  active_arr = elements;
  
  document.body.appendChild(hints);
}

function dwb_update_hints(input) {
  var array = [];
  var text_content;
  var keep = false;
  if (input) {
    input = input.toLowerCase();
  }
  if (lastinput && (lastinput.length > input.length)) {
    dwb_clear();
    lastinput = input;
    dwb_show_hints();
    dwb_update_hints(input);
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
    dwb_clear();
    return "_dwb_no_hints_";
  }
  else if (array.length == 1) {
    return evaluate(array[0]);
  }
  else {
    lastpos = array[0].betterMatch(input);
    dwb_set_active(array[lastpos]);
  }
}
function dwb_set_active(element) {
  var active = document.querySelector('*[dwb_highlight=hint_active]');
  if (active) {
    active.setAttribute('dwb_highlight', 'hint_normal' );
  }
  element.element.setAttribute('dwb_highlight', 'hint_active');
  active = element;
}
function dwb_clear() {
  if (elements) {
    for (var i=0; i<elements.length; i++) {
      elements[i].element.removeAttribute('dwb_highlight');
    }
  }
  var hints = document.getElementById("dwb_hints");
  if (hints) 
    hints.parentNode.removeChild(hints);
  elements = [];
  active_arr = [];
}

function evaluate(element) {
  var ret, type;
  var e = element.element;
  if (e.type) 
   type = e.type.toLowerCase();
  var tagname = e.tagName.toLowerCase();

  if (tagname && (tagname == "input" || tagname == "textarea") ) {
    if (type == "radio" || type == "checkbox") {
      e.focus();
      dwb_click_element(element, "click");
      ret = "_dwb_check_";
    }
    else if (type == "submit" || type == "reset" || type  == "button") {
      dwb_click_element(element, "click");
      ret = "_dwb_click_";
    }
    else {
      e.focus();
      ret = "_dwb_input_";
    }
  }
  else {
    if (tagname == "a" || e.hasAttribute("onclick")) 
      dwb_click_element(element, "click");
    else if (e.hasAttribute("onmousedown")) 
      dwb_click_element(element, "mousedown");
    else {
      dwb_click_element(element);
    }
    ret = "_dwb_click_";
  }
  return ret;
}
function dwb_get_active() {
  return evaluate(active);
}

function dwb_focus_next() {
  var newpos = lastpos == active_arr.length-1 ? 0 : lastpos + 1;
  active = active_arr[newpos];
  dwb_set_active(active);
  lastpos = newpos;
}
function dwb_focus_prev() {
  var newpos = lastpos == 0 ? active_arr.length-1 : lastpos - 1;
  active = active_arr[newpos];
  dwb_set_active(active);
  lastpos = newpos;
}
function dwb_func_with_args(command, letter_seq, font_size, font_weight, font_family, style,
    fg_color, bg_color, active_color, normal_color, border,  opacity) {
  hint_letter_seq  = letter_seq;
  hint_font_size = font_size;
  hint_font_weight = font_weight;
  hint_font_family = font_family;
  hint_style =  style;
  hint_fg_color    = fg_color;
  hint_bg_color    = bg_color;
  hint_active_color = active_color;
  hint_normal_color = normal_color;
  hint_border = border;
  hint_opacity = opacity;
  command();
}
function dwb_add_searchengine() {
  dwb_create_stylesheet();
  var hints = document.createElement("div");
  var res = document.body.querySelectorAll("form");

  for (var i=0; i<res.length; i++) {
    var els = res[i].elements;
    for (var j=0; j<els.length; j++) {
      if (dwb_get_visibility(els[j]) && (els[j].type == "text" || els[j].type == "search")) {
        var e = new DwbLetterHint(els[j]);
        elements.push(e);
        e.element.setAttribute('dwb_highlight', 'hint_normal');
      }
    }
  }
  if (!elements.length) {
    return "_dwb_no_hints_";
  }
  for (var i=0; i<elements.length; i++) {
    elements[i].getTextHint(i, elements.length);
    elements[i].element.setAttribute('dwb_highlight', 'hint_normal');
  }
  document.body.appendChild(hints); 
  dwb_set_active[elements[0]];
  active_arr = elements;
  dwb_focus_next();
}
function dwb_submit_searchengine(string) {
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
function dwb_focus_input() {
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
function dwb_blur() {
  document.activeElement.blur();
}
dwb_clear();
