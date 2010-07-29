var elements = [];
var active_arr = [];
var hints;
var overlays;
var active;
var last_active;
var lastpos = 0;
var lastinput;
var styles;

function Hint(element) {
  this.element = element;
  this.rect = element.getBoundingClientRect();

  function create_span(element, h, v) {
    var span = document.createElement("span");
    var leftpos = Math.max((element.rect.left + document.defaultView.scrollX), document.defaultView.scrollX) + h;
    var toppos = Math.max((element.rect.top + document.defaultView.scrollY), document.defaultView.scrollY) + v;
    span.style.position = "absolute";
    span.style.left = leftpos + "px";
    span.style.top = toppos + "px";
    return span;
  }
  function create_hint(element) {
    var hint = create_span(element, hint_horizontal_off, hint_vertical_off - element.rect.height/2);
    hint.style.fontSize = hint_font_size;
    hint.style.fontWeight = hint_font_weight;
    hint.style.fontFamily = hint_font_family;
    hint.style.color = hint_fg_color;
    hint.style.background = hint_bg_color;
    hint.style.opacity = hint_opacity;
    hint.style.border = hint_border;
    hint.style.zIndex = 2;
    hint.style.visibility = 'visible';
    return hint;
  }
  function create_overlay(element) {
    var overlay = create_span(element, 0, 0);
    overlay.style.width = (element.rect.right - element.rect.left) + "px";
    overlay.style.height = (element.rect.bottom - element.rect.top) + "px";
    overlay.style.opacity = overlay_opacity;
    overlay.style.backgroundColor = hint_normal_color;
    overlay.style.border = overlay_border;
    overlay.style.zIndex = 1;
    overlay.style.visibility = 'visible';
    overlay.addEventListener( 'click', function() { click_element(element); }, false );
    return overlay;
  }

  this.hint = create_hint(this);
  this.overlay = create_overlay(this);
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
  else {
    text = hint_letter_seq[i%l] + hint_letter_seq[l - 1 - (parseInt(i/l))];
  }
  var content = document.createTextNode(text);
  this.hint.appendChild(content);
}

LetterHint.prototype.betterMatch = function(input) {
  for (var i=0; i<active_arr.length; i++) {
    var e = active_arr[i];
    if (e.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) == 0) {
      return i;
    }
  }
}

LetterHint.prototype.matchText = function(input) {
  return this.hint.textContent.toLowerCase().match(input.toLowerCase());
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

function show_hints() {
  document.activeElement.blur();
  var res = document.body.querySelectorAll('a, area, textarea, select, link, input:not([type=hidden]), button,  frame, iframe');
  hints = document.createElement("div");
  overlays  = document.createElement("div");
  for (var i=0; i<res.length; i++) {
    var e = new LetterHint(res[i]);
    hints.appendChild(e.hint);
    overlays.appendChild(e.overlay);
    var rects = e.element.getClientRects()[0];
    var r = e.rect;
    if (!r || r.top > window.innerHeight || r.bottom < 0 || r.left > window.innerWidth ||  r < 0 || !rects ) {
      continue;
    }
    var style = document.defaultView.getComputedStyle(e.element, null);
    if (style.getPropertyValue("visibility") != "visible" || style.getPropertyValue("display") == "none") {
      continue;
    }
    elements.push(e);
  };
  elements.sort( function(a,b) { return a.rect.top - b.rect.top; });
  for (var i=0; i<elements.length; i++) {
    var e = elements[i];
    e.getTextHint(i, elements.length);
  }
  active_arr = elements;
  document.body.appendChild(hints);
  document.body.appendChild(overlays);
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
      e.overlay.style.visibility = 'hidden';
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
    array[lastpos].overlay.style.backgroundColor = hint_normal_color;
    lastpos = array[0].betterMatch(input);
    array[lastpos].overlay.style.backgroundColor = hint_active_color;
  }
}
function clear() {
  if (overlays && overlays.parentNode) {
    overlays.parentNode.removeChild(overlays);
  }
  if (hints  && hints.parentNode) {
    hints.parentNode.removeChild(hints);
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
      //e.checked = !e.checked;
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
  active_arr[lastpos].overlay.style.backgroundColor = hint_normal_color;
  active_arr[newpos].overlay.style.backgroundColor = hint_active_color;
  active = active_arr[newpos];
  lastpos = newpos;
}
function focus_prev() {
  var newpos = lastpos == 0 ? active_arr.length-1 : lastpos - 1;
  active_arr[lastpos].overlay.style.backgroundColor = hint_normal_color;
  active_arr[newpos].overlay.style.backgroundColor = hint_active_color;
  active = active_arr[newpos];
  lastpos = newpos;
}
function add_searchengine() {
  document.activeElement.blur();
  var res = document.body.querySelectorAll('form');
  hints = document.createElement("div");
  overlays  = document.createElement("div");

  for (var i=0; i<res.length; i++) {
    var els = res[i].elements;
    for (var j=0; j<els.length; j++) {
      if (els[j].type == "text") {
        var e = new LetterHint(els[j]);
        hints.appendChild(e.hint);
        overlays.appendChild(e.overlay);
        elements.push(e);
      }
    }
  }
  if (!elements.length) {
    return "_dwb_no_hints_";
  }
  elements.sort( function(a,b) { return a.rect.top - b.rect.top; });
  for (var i=0; i<elements.length; i++) {
    elements[i].getTextHint(i, elements.length);
  }
  active_arr = elements;
  document.body.appendChild(hints); 
  document.body.appendChild(overlays); 
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
