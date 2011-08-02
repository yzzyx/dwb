String.prototype.isInt = function() { return !isNaN(parseInt(this)); }
DwbHintObj = (function() {
  var _hints = null;
  _letterSeq = "FDSARTGBVECWXQYIOPMNHZULKJ";
  _fontSize = "12px";
  _fontWeight = "normal";
  _fontFamily = "monospace";
  _style = "letter";
  _fgColor = "#ffffff";
  _bgColor = "#000088";
  _activeColor = "#00ff00";
  _normalColor = "#ffff99";
  _hintBorder = "2px dashed #000000";
  _hintOpacity = 0.65;
  _elements = [];
  _activeArr = [];
  _lastInput = null;
  _lastPosition = 0;
  _activeInput = null;
  _styles = 0;
  _hintTypes = "a, img, textarea, select, link, input:not([type=hidden]), button,  frame, iframe, *[onclick], *[onmousedown], *[role=link]";

  const __newHint = function (element, win, offset, rect) {
      this.element = element;
      this.win = win;
      if (!offset) {
        offset = [ 0, 0 ];
      }

      function create_span(element) {
        var span = __createElement("span");
        var leftpos, toppos;
        if (element instanceof HTMLAreaElement) {
          var coords = element.coords.split(",");
          var leftpos = offset[0] + document.defaultView.scrollX + parseInt(coords[0]);
          var toppos = offset[1] + document.defaultView.scrollY + parseInt(coords[1]);
        }
        else if (rect) {
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
        hint.style.fontSize = _fontSize;
        hint.style.fontWeight = _fontWeight;
        hint.style.fontFamily = _fontFamily;
        hint.style.color = _fgColor;
        hint.style.background = _bgColor;
        hint.style.opacity = _hintOpacity;
        hint.style.border = _hintBorder;
        hint.style.zIndex = 20000;
        hint.style.visibility = 'visible';
        hint.name = "dwb_hint";

        return hint;
      }

      this.hint = create_hint(element);
    }
  const __createElement = function(tagname) {
    element = document.createElement(tagname);
    if (!element.style) {
      var namespace = document.getElementsByTagName('html')[0].getAttribute('xmlns') || "http://www.w3.org/1999/xhtml";
      element = document.createElementNS(namespace, tagname);
    }
    return element;
  }
  var __numberHint = function (element, win, offset, rect) {
    this.constructor = __newHint;
    this.constructor(element, win, offset, rect);

    this.getTextHint = function (i, length) {
      start = length <=10 ? 1 : length <= 100 ? 10 : 100;
      var content = document.createTextNode(start + i);
      this.hint.appendChild(content);
    }

    this.betterMatch = function(input) {
      if (input.isInt()) {
        return 0;
      }
      var bestposition = 37;
      var ret = 0;
      for (var i=0; i<_activeArr.length; i++) {
        var e = _activeArr[i];
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
    this.matchText = function(input) {
      var ret = false;
      if (input.isInt()) {
        var regEx = new RegExp('^' + input);
        return regEx.test(this.hint.textContent);
      }
      else {
        return this.element.textContent.toLowerCase().match(input);
      }
    }
  };
  const __letterHint = function (element, win, offset, rect) {
    this.constructor = __newHint;
    this.constructor(element, win, offset, rect);

    this.betterMatch = function(input) {
      return 0;
    }
    this.getTextHint = function(i, length) {
      var text;
      var l = _letterSeq.length;
      if (length < l) {
        text = _letterSeq[i];
      }
      else if (length < 2*l) {
        var rem = (length) % l;
        var sqrt = Math.sqrt(2*rem);
        var r = sqrt == (getint = parseInt(sqrt)) ? sqrt + 1 : getint;
        if (i < l-r) {
          text = _letterSeq[i];
        }
        else {
          var newrem = i%(r*r);
          text = _letterSeq[Math.floor( (newrem / r) + l - r )] + _letterSeq[l-newrem%r - 1];
        }
      }
      else {
        text = _letterSeq[i%l] + _letterSeq[l - 1 - (parseInt(i/l))];
      }
      var content = document.createTextNode(text);
      this.hint.appendChild(content);
    }
    this.matchText = function(input) {
      return (this.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) == 0);
    }
  }

  const __mouseEvent = function (e, ev) {
    var mouseEvent = document.createEvent("MouseEvent");
    mouseEvent.initMouseEvent(ev, true, true, e.win, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
    e.dispatchEvent(mouseEvent);
  }
  const __clickElement = function (element, ev) {
    if (ev) {
      __mouseEvent(element, ev);
    }
    else { // both events, if no event is given
      __mouseEvent(element, "click");
      __mouseEvent(element, "mousedown");
    }
    __clear();
  }
  const __getActive = function () {
    return document.querySelector('*[dwb_highlight=hint_active]');
  }
  const __setActive = function (element) {
    var active = __getActive();
    if (active) {
      active.setAttribute('dwb_highlight', 'hint_normal' );
    }
    element.element.setAttribute('dwb_highlight', 'hint_active');
  }
  const __hexToRgb = function (color) {
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
    return "rgba(" + rgb.slice(1) + "," +  _hintOpacity + ")";
  }
  const __createStyleSheet = function() {
    if (_styles != 0)
      return;
    _styles = 1;
    var style = document.styleSheets[document.styleSheets.length - 1];
    style.insertRule('*[dwb_highlight=hint_normal] { background-color: ' + _normalColor + ' !important; } ', 0);
    style.insertRule('*[dwb_highlight=hint_active] { background-color: ' + _activeColor + ' !important; } ', 0);
  }
  const __showHints = function () {
    if (document.activeElement) 
      document.activeElement.blur();

    __createStyleSheet();

    _hints = document.createDocumentFragment();
    var constructor = _style == "letter" ? __letterHint : __numberHint;

    var res = document.body.querySelectorAll(_hintTypes); 
    for (var i=0; i<res.length; i++) {
      __getElement(window, res[i], null, constructor);
    };
    for (var i=0; i<_elements.length; i++) {
      if (res[i] == _elements[i]) {
        continue;
      }
      var e = _elements[i];
      _hints.appendChild(e.hint);
      e.getTextHint(i, _elements.length);
      e.element.setAttribute('dwb_highlight', 'hint_normal');
      e.hint.style.visibility = 'visible';
    }
    _activeArr = _elements;
    __setActive(_elements[0]);
    document.body.appendChild(_hints);
  }
  const __updateHints = function(input) {
    var array = [];
    var text_content;
    var keep = false;
    if (!_activeArr.length) {
      __clear();
      __showHints();
    }
    if (input) {
      input = input.toLowerCase();
    }
    if (_lastInput && (_lastInput.length > input.length)) {
      __clear();
      _lastInput = input;
      __showHints();
      __updateHints(input);
      return;
    }
    _lastInput = input;
    if (_style == "number" && input) {
      if (input[input.length-1].isInt()) {
        input = input.match(/[0-9]+/g).join("");
      }
      else {
        input = input.match(/[^0-9]+/g).join("");
      }
    }
    for (var i=0; i<_activeArr.length; i++) {
      var e = _activeArr[i];
      if (e.matchText(input)) {
        array.push(e);
      }
      else {
        e.hint.style.visibility = 'hidden';
        e.element.removeAttribute('dwb_highlight');
      }
    }
    _activeArr = array;
    active = array[0];
    if (array.length == 0) {
      __clear();
      return "_dwb_no_hints_";
    }
    else if (array.length == 1) {
      return __evaluate(array[0].element);
    }
    else {
      _lastPosition = array[0].betterMatch(input);
      __setActive(array[_lastPosition]);
    }
  }
  const __getVisibility = function (e) {
    var style = document.defaultView.getComputedStyle(e, null);
    if ((style.getPropertyValue("visibility") == "hidden" || style.getPropertyValue("display") == "none" ) ) {
      return null;
    }
    var rects = e.getClientRects()[0];
    var r = e.getBoundingClientRect();

    var height = window.innerHeight ? window.innerHeight : document.body.offsetHeight;
    var width = window.innerWidth ? window.innerWidth : document.body.offsetWidth;
    if (!r || r.top > height || r.bottom < 0 || r.left > width ||  r.right < 0 || !rects) {
      return null;
    }
    return r;
  }
  const __getElement = function (win, e, offset, constructor) {
    var leftoff = 0;
    var topoff = 0;
    var r;
    if (offset) {
      leftoff += offset[0];
      topoff += offset[1];
    }
    if (e instanceof HTMLImageElement) {
      if (!e.useMap) 
        return;
      var area = document.getElementById(e.useMap.slice(1));
      if (!area)
        return;
      var areas = area.getElementsByTagName("area");
      var r = e.getBoundingClientRect();
      for (var i=0; i<areas.length; i++) {
        var element = new constructor(areas[i], win, [leftoff + r.left, topoff + r.top], r);
        _elements.push(element);
      }
    }
    if (r = __getVisibility(e)) {
      if ( (e instanceof HTMLFrameElement || e instanceof HTMLIFrameElement)) {
        const doc = e.contentDocument ? e.contentDocument : e.contentWindow.document;
        if (doc) {
          var res = doc.body.querySelectorAll(_hintTypes);
          var off = [ leftoff + e.offsetLeft, topoff + e.offsetTop ];
          for (var i=0; i < res.length; i++) {
            __getElement(e, res[i], off, constructor);
          }
        }
      }
      else {
        var off = [ leftoff, topoff ];
        var element = new constructor(e, win, off, r);
        _elements.push(element);
      }
    }
  }
  const __clear = function() {
    if (_elements) {
      for (var i=0; i<_elements.length; i++) {
        _elements[i].element.removeAttribute('dwb_highlight');
        document.body.removeChild(_elements[i].hint);
      }
    }
    if (_hints) 
      _hints = null;
    _elements = [];
    _activeArr = [];
  }
  const __evaluate = function (e) {
    var ret, type;
    if (e.type) 
      type = e.type.toLowerCase();
    var tagname = e.tagName.toLowerCase();

    if (tagname && (tagname == "input" || tagname == "textarea") ) {
      if (type == "radio" || type == "checkbox") {
        e.focus();
        __clickElement(e, "click");
        ret = "_dwb_check_";
      }
      else if (type == "submit" || type == "reset" || type  == "button") {
        __clickElement(e, "click");
        ret = "_dwb_click_";
      }
      else {
        e.focus();
        ret = "_dwb_input_";
      }
    }
    else {
      if (tagname == "a" || e.hasAttribute("onclick")) 
        __clickElement(e, "click");
      else if (e.hasAttribute("onmousedown")) 
        __clickElement(e, "mousedown");
      else {
        __clickElement(e);
      }
      ret = "_dwb_click_";
    }
    __clear();
    return ret;
  }
  const __focusNext = function()  {
    var newpos = _lastPosition == _activeArr.length-1 ? 0 : _lastPosition + 1;
    active = _activeArr[newpos];
    __setActive(active);
    _lastPosition = newpos;
  }
  const __focusPrev = function()  {
    var newpos = _lastPosition == 0 ? _activeArr.length-1 : _lastPosition - 1;
    active = _activeArr[newpos];
    __setActive(active);
    _lastPosition = newpos;
  }
  const __addSearchEngine = function() {
    __createStyleSheet();
    var hints = __createElement("div");
    var res = document.body.querySelectorAll("form");

    for (var i=0; i<res.length; i++) {
      var els = res[i].elements;
      for (var j=0; j<els.length; j++) {
        if (__getVisibility(els[j]) && (els[j].type == "text" || els[j].type == "search")) {
          var e = new __letterHint(els[j], window);
          _elements.push(e);
          e.element.setAttribute('dwb_highlight', 'hint_normal');
        }
      }
    }
    if (!_elements.length) {
      return "_dwb_no_hints_";
    }
    for (var i=0; i<_elements.length; i++) {
      _elements[i].getTextHint(i, _elements.length);
      _elements[i].element.setAttribute('dwb_highlight', 'hint_normal');
    }
    document.body.appendChild(hints); 
    __setActive(_elements[0]);
    _activeArr = _elements;
  }
  const __submitSearchEngine = function (string) {
    var e = __getActive();
    e.value = string;
    e.form.submit();
    e.value = "";
    if (e.form.method.toLowerCase() == 'post') {
      return e.name;
    }
    return NULL;
  }
  const __focusInput = function() {
    var res = document.body.querySelectorAll('input[type=text], input[type=password], textarea');
    if (res.length == 0) {
      return "_dwb_no_input_";
    }
    styles = document.styleSheets[0];
    styles.insertRule('input:focus { outline: 2px solid #1793d1; }', 0);
    if (!_activeInput) {
      _activeInput = res[0];
    }
    else {
      for (var i=0; i<res.length; i++) {
        if (res[i] == _activeInput) {
          if (!(_activeInput = res[i+1])) {
            _activeInput = res[0];
          }
          break;
        }
      }
    }
    _activeInput.focus();
  }


  return {
  createStyleSheet : 
    function () {
      __createStyleSheet();
    },
  showHints : 
    function() {
      __showHints();
    },

  updateHints :
    function (input) {
      __updateHints(input);
    },
  clear : 
    function () {
      __clear();
    },
  followActive :
    function () {
      return __evaluate(__getActive());
    },

  focusNext :
    function () {
      __focusNext();
    },
  focusPrev : 
    function () {
      __focusPrev();
    },
  addSearchEngine : 
    function () {
      __addSearchEngine();
    },
  submitSearchEngine :
    function (string) {
      __submitSearchEngine(string);
    },
  focusInput : 
    function () {
    },
  init: 
    function (letter_seq, font_size, font_weight, font_family, style,
        fg_color, bg_color, active_color, normal_color, border,  opacity) {
      _letterSeq  = letter_seq;
      _fontSize = font_size;
      _fontWeight = font_weight;
      _fontFamily = font_family;
      _style =  style.toLowerCase();
      _fgColor    = fg_color;
      _bgColor    = bg_color;
      _activeColor = __hexToRgb(active_color);
      _normalColor = __hexToRgb(normal_color);
      _hintBorder = border;
      _hintOpacity = opacity;
    }, 
  }
})();
