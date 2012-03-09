String.prototype.isInt = function () { 
  return !isNaN(parseInt(this, 10)); 
};
String.prototype.isLower = function () { 
  return this == this.toLowerCase(); 
};
var DwbHintObj = (function () {
  var _letterSeq = "FDSARTGBVECWXQYIOPMNHZULKJ";
  var _font = "bold 10px monospace";
  var _style = "letter";
  var _fgColor = "#ffffff";
  var _bgColor = "#000088";
  var _activeColor = "#00ff00";
  var _normalColor = "#ffff99";
  var _hintBorder = "2px dashed #000000";
  var _hintOpacity = 0.65;
  var _elements = [];
  var _activeArr = [];
  var _lastInput = null;
  var _lastPosition = 0;
  var _activeInput = null;
  var _new_tab = false;
  var _active = null;
  var _markHints = false;
  var _bigFont = null;
  var _hintTypes =  [ "a, map, textarea, select, input:not([type=hidden]), button,  frame, iframe, [onclick], [onmousedown]," + 
      "[onmouseover], [role=link], [role=option], [role=button], [role=option]",  // HINT_T_ALL
                 //[ "iframe", 
                 "a",  // HINT_T_LINKS
                 "img",  // HINT_T_IMAGES
                 "input[type=text], input[type=password], input[type=search], textarea",  // HINT_T_EDITABLE
                 "[src], [href]"  // HINT_T_URL
               ];
  var HintTypes = {
    HINT_T_ALL : 0,
    HINT_T_LINKS : 1,
    HINT_T_IMAGES : 2,
    HINT_T_EDITABLE : 3,
    HINT_T_URL : 4
  };
  var OpenMode = {
    OPEN_NORMAL      : 1<<0, 
    OPEN_NEW_VIEW    : 1<<1, 
    OPEN_NEW_WINDOW  : 1<<2
  };

  var __getTextHints = function(arr) {
    var length = arr.length;
    var i, j, e, text, cur, start, l, max, r;
    if (length === 0)
      return;
    if (arr[0] instanceof __numberHint) {
      for (i=0; i<length; i++) {
        e = arr[i];
        start = e.getStart(length);
        e.hint.textContent = start+i;
      }
    }
    else if (arr[0] instanceof __letterHint) {
      l = _letterSeq.length;
      max = Math.ceil(Math.log(length)/Math.log(l));
      r = Math.ceil(Math.pow(length, 1/max));
      for (i=0; i<length; i++) {
        e = arr[i];
        text = new String();
        cur = i;
        for (j=0; j<max; j++) {
          text += _letterSeq[(cur%r)];
          cur = Math.floor(cur/r);
        }
        e.hint.textContent = text;
      }
    }
  };

  var __newHint = function(element, win, rect, oe) {
    this.element = element;
    this.overlay = null;
    this.win = win;
    var hint = __createElement("div");
    var toppos = rect.top + oe.offY;
    var leftpos = rect.left + oe.offX;
    var t = Math.max(toppos, 0);
    var l = Math.max(leftpos, 0);
    hint.style.top = t + "px";
    hint.style.marginTop = oe.marginTop;
    hint.style.left = l + "px";
    hint.style.marginLeft = oe.marginLeft;

    hint.className =  "dwb_hint";
    this.createOverlay = function() {
      var comptop = toppos;
      var compleft = leftpos;
      var height = rect.height;
      var width = rect.width;
      var h = height + Math.max(0, comptop);
      var overlay = __createElement("div");
      overlay.className = "dwb_overlay_normal";
      overlay.style.width = (compleft > 0 ? width : width + compleft) + "px";
      overlay.style.height = (comptop > 0 ? height : height + comptop) + "px";
      overlay.style.top = t + "px";
      overlay.style.marginTop = oe.marginTop;
      overlay.style.left = l + "px";
      overlay.style.marginLeft = oe.marginLeft;
      overlay.style.display = "block";
      overlay.style.cursor = "pointer";
      this.overlay = overlay;
    };
    this.hint = hint;
  };
  var __createElement = function(tagname) {
    var element = document.createElement(tagname);
    if (!element.style) {
      var namespace = document.getElementsByTagName('html')[0].getAttribute('xmlns') || "http://www.w3.org/1999/xhtml";
      element = document.createElementNS(namespace, tagname);
    }
    return element;
  };
  var __numberHint = function (element, win, rect, offsetElement) {
    this.varructor = __newHint;
    this.varructor(element, win, rect, offsetElement);

    this.getStart = function(n) {
      var start = parseInt(Math.log(n) / Math.log(10), 10)*10;
      if (n > 10*start-start) {
        start*=10;
      }
      return Math.max(start, 1);
    };
    this.betterMatch = function(input) {
      var length = _activeArr.length;
      var i, cl;
      if (input.isInt()) {
        return 0;
      }
      var bestposition = 37;
      var ret = 0;
      for (i=0; i<length; i++) {
        var e = _activeArr[i];
        if (input && bestposition !== 0) {
          var content = e.element.textContent.toLowerCase().split(" ");
          for (cl=0; cl<content.length; cl++) {
            if (content[cl].toLowerCase().indexOf(input) === 0) {
              if (cl < bestposition) {
                ret = i;
                bestposition = cl;
                break;
              }
            }
          }
        }
      }
      __getTextHints(_activeArr);
      return ret;
    };
    this.matchText = function(input, matchHint) {
      var i;
      if (matchHint) {
        var regEx = new RegExp('^' + input);
        return regEx.test(this.hint.textContent);
      }
      else {
        var inArr = input.split(" ");
        for (i=0; i<inArr.length; i++) {
          if (!this.element.textContent.toLowerCase().match(inArr[i].toLowerCase())) {
            return false;
          }
        }
        return true;
      }
    };
  };
  var __letterHint = function (element, win, rect, offsetElement) {
    this.varructor = __newHint;
    this.varructor(element, win, rect, offsetElement);

    this.betterMatch = function(input) {
      return 0;
    };
    this.matchText = function(input, matchHint) {
      var i;
      if (matchHint) {
        return (this.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) === 0);
      }
      else {
        var inArr = input.split(" ");
        for (i=0; i<inArr.length; i++) {
          if (!this.element.textContent.toUpperCase().match(inArr[i])) {
            return false;
          }
        }
        return true;
      }
    };
  };

  var __mouseEvent = function (e, ev, bubble) {
    if (e.ownerDocument != document) {
      e.focus();
    }
    var mouseEvent = e.ownerDocument.createEvent("MouseEvent");
    mouseEvent.initMouseEvent(ev, bubble, true, e.ownerDocument.defaultView, 0, 0, 0, 0, 0, false, false, false, false, 
        _new_tab & OpenMode.OPEN_NEW_VIEW ? 1 : 0, null);
    e.dispatchEvent(mouseEvent);
  };
  var __clickElement = function (element, ev) {
    if (ev) {
      __mouseEvent(element, ev, !_new_tab);
    }
    else { // both events, if no event is given
      __mouseEvent(element, "click", !_new_tab);
      __mouseEvent(element, "mousedown", !_new_tab);
    }
  };
  var __getActive = function () {
    return _active;
  };
  var __setActive = function (element) {
    var active = __getActive();
    if (active) {
      if (_markHints) {
        active.overlay.style.background = _normalColor;
      }
      else if (active.overlay) {
        active.overlay.parentNode.removeChild(active.overlay);
      }
      active.hint.style.font = _font;

    }
    _active = element;
    if (!_active.overlay) {
      _active.createOverlay();
    }
    if (!_markHints) {
      _active.hint.parentNode.appendChild(_active.overlay);
    }
    _active.overlay.style.background = _activeColor;
    _active.hint.style.fontSize = _bigFont;
  };
  var __hexToRgb = function (color) {
    var rgb, i;
    if (color[0] !== '#') {
      return color;
    }
    if (color.length == 4) {
      rgb = /#([0-9a-f])([0-9a-f])([0-9a-f])/i.exec(color);
      for (i=1; i<=3; i++) {
        var v = parseInt("0x" + rgb[i], 10)+1;
        rgb[i] = v*v-1;
      }
    }
    else {
      rgb  = /#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})/i.exec(color);
      for (i=1; i<=3; i++) {
        rgb[i] = parseInt("0x" + rgb[i], 16);
      }
    }
    return "rgba(" + rgb.slice(1) + "," +  _hintOpacity/2 + ")";
  };
  var __createStyleSheet = function(doc) {
    if (doc.hasStyleSheet) {
      return;
    }
    var styleSheet = __createElement("style");
    styleSheet.innerHTML += ".dwb_hint { " +
      "position:absolute; z-index:20000;" +
      "background:" + _bgColor  + ";" + 
      "color:" + _fgColor + ";" + 
      "border:" + _hintBorder + ";" + 
      "font:" + _font + ";" + 
      "display:inline;" +
      "opacity: " + _hintOpacity + "; }" + 
      ".dwb_overlay_normal { position:absolute!important;display:block!important; z-index:19999;background:" + _normalColor + ";}";
    doc.head.appendChild(styleSheet);
    doc.hasStyleSheet = true;
  };
  var __getOffsets = function(doc) {
    var oe = new Object();
    var win = doc.defaultView;
    var body = doc.body || doc.documentElement;
    var bs = win.getComputedStyle(body, null);
    var br = body.getBoundingClientRect();
    if (bs && br && (/^(relative|fixed|absolute)$/.test(bs.position)) ) {
      oe.offX = -br.left; 
      oe.offY = -br.top;
      oe.marginTop = bs.marginTop;
      oe.marginLeft = bs.marginLeft;
    }
    else {
      oe.offX = win.pageXOffset;
      oe.offY = win.pageYOffset;
      oe.marginTop = 0 + "px";
      oe.marginLeft = 0 + "px";
    }
    return oe;
  };
  var __createHints = function(win, varructor, type) {
    var i;
    try {
      var doc = win.document;
      var res = doc.body.querySelectorAll(_hintTypes[type]); 
      var e, r;
      __createStyleSheet(doc);
      var hints = doc.createDocumentFragment();
      var oe = __getOffsets(doc);
      for (i=0;i < res.length; i++) {
        e = res[i];
        if ( (e instanceof HTMLFrameElement || e instanceof HTMLIFrameElement)) {
          __createHints(e.contentWindow, varructor, type);
          continue;
        }
        if ((r = __getVisibility(e, win)) === null) {
          continue;
        }
        else {
          var element = new varructor(e, win, r, oe);
          _elements.push(element);
          hints.appendChild(element.hint);
          if (_markHints) {
            element.createOverlay();
            hints.appendChild(element.overlay);
          }
        }
      }
      doc.body.appendChild(hints);
    }
    catch(exc) {
      console.error(exc);
    }
  };
  var __showHints = function (type, new_tab) {
    var i;
    if (document.activeElement) {
      document.activeElement.blur();
    }

    _new_tab = new_tab;
    __createHints(window, _style == "letter" ? __letterHint : __numberHint, type);
    var l = _elements.length;

    if (l === 0) {
      return "_dwb_no_hints_";
    }
    else if (l == 1)  {
      return  __evaluate(_elements[0].element, type);
    }

    __getTextHints(_elements);
    _activeArr = _elements;
    __setActive(_elements[0]);
    return null;
  };
  var __updateHints = function(input, type) {
    var i;
    var array = [];
    var matchHint = false;
    if (!_activeArr.length) {
      __clear();
      __showHints(type, _new_tab);
    }
    if (_lastInput && (_lastInput.length > input.length)) {
      __clear();
      _lastInput = input;
      __showHints(type, _new_tab);
      return __updateHints(input, type);
    }
    _lastInput = input;
    if (input) {
      if (_style == "number") {
        if (input[input.length-1].isInt()) {
          input = input.match(/[0-9]+/g).join("");
          matchHint = true;
        }
        else {
          input = input.match(/[^0-9]+/g).join("");
        }
      }
      else if (_style == "letter") {
        var lowerSeq = _letterSeq.toLowerCase();
        if (input[input.length-1].isLower()) {
          if (lowerSeq.indexOf(input.charAt(input.length-1)) == -1) {
            return "_dwb_no_hints_";
          }
          input = input.match(new RegExp("[" + lowerSeq + "]", "g")).join("");
          matchHint = true;
        }
        else  {
          input = input.match(new RegExp("[^" + lowerSeq + "]", "g")).join("");
        }
      }
    }
    for (i=0; i<_activeArr.length; i++) {
      var e = _activeArr[i];
      if (e.matchText(input, matchHint)) {
        array.push(e);
      }
      else {
        e.hint.style.visibility = 'hidden';
      }
    }
    _activeArr = array;
    if (array.length === 0) {
      __clear();
      return "_dwb_no_hints_";
    }
    else if (array.length == 1) {
      return __evaluate(array[0].element, type);
    }
    else {
      _lastPosition = array[0].betterMatch(input);
      __setActive(array[_lastPosition]);
    }
    return null;
  };
  var __getVisibility = function (e, win) {
    var style = win.getComputedStyle(e, null);
    if ((style.getPropertyValue("visibility") == "hidden" || style.getPropertyValue("display") == "none" ) ) {
      return null;
    }
    var r = e.getBoundingClientRect();

    var height = win.innerHeight || document.body.offsetHeight;
    var width = win.innerWidth || document.body.offsetWidth;
    if (!r || r.top > height || r.bottom < 0 || r.left > width ||  r.right < 0 || !e.getClientRects()[0]) {
      return null;
    }
    return r;
  };
  var __clear = function() {
    var p, i;
    try {
      if (_elements) {
        for (i=0; i<_elements.length; i++) {
          var e = _elements[i];
          if ( (p = e.hint.parentNode) ) {
            p.removeChild(e.hint);
          }
          if (e.overlay && (p = e.overlay.parentNode)) {
            p.removeChild(e.overlay);
          }
        }
      }
      if(! _markHints && _active) {
        _active.element.removeAttribute("dwb_highlight");
      }
    }
    catch (exc) { 
      console.error(exc); 
    }
    _elements = [];
    _activeArr = [];
    _active = null;
    _lastPosition = 0;
    _lastInput = null;
  };
  var __evaluate = function (e, type) {
    var ret = null;
    var elementType = null;
    if (e.type) {
      elementType = e.type.toLowerCase();
    }
    var tagname = e.tagName.toLowerCase();
    if (_new_tab && e.target == "_blank") {
      e.target = null;
    }
    if (type > 0) {
      switch (type) {
        case HintTypes.HINT_T_IMAGES:  ret = e.src; break;
        case HintTypes.HINT_T_URL:     ret = e.hasAttribute("href") ? e.href : e.src; break;
        default: break;
      }
    }
    else if ((tagname && (tagname == "input" || tagname == "textarea"))) {
      if (type == "radio" || type == "checkbox") {
        e.focus();
        __clickElement(e, "click");
        ret = "_dwb_check_";
      }
      else if (elementType && (elementType == "submit" || elementType == "reset" || elementType  == "button")) {
        __clickElement(e, "click");
        ret = "_dwb_click_";
      }
      else {
        e.focus();
        ret = "_dwb_input_";
      }
    }
    else if (e.hasAttribute("role")) {
      __clickElement(e);
      ret = "_dwb_click_";
    }
    else {
      if (tagname == "a" || e.hasAttribute("onclick")) {
        __clickElement(e, "click");
      }
      else if (e.hasAttribute("onmousedown")) {
        __clickElement(e, "mousedown");
      }
      else {
        __clickElement(e);
      }
      ret = "_dwb_click_";
    }
    __clear();
    return ret;
  };
  var __focusNext = function()  {
    var newpos = _lastPosition === _activeArr.length-1 ? 0 : _lastPosition + 1;
    __setActive(_activeArr[newpos]);
    _lastPosition = newpos;
  };
  var __focusPrev = function()  {
    var newpos = _lastPosition === 0 ? _activeArr.length-1 : _lastPosition - 1;
    __setActive(_activeArr[newpos]);
    _lastPosition = newpos;
  };
  var __addSearchEngine = function() {
    var i, j;
    try {
      __createStyleSheet(document);
      var hints = document.createDocumentFragment();
      var res = document.body.querySelectorAll("form");
      var r, e;
      var oe = __getOffsets(document);

      for (i=0; i<res.length; i++) {
        var els = res[i].elements;
        for (j=0; j<els.length; j++) {
          if (((r = __getVisibility(els[j], window)) !== null) && (els[j].type === "text" || els[j].type === "search")) {
            e = new __letterHint(els[j], window, r, oe);
            hints.appendChild(e.hint);
            e.hint.style.visibility = "hidden";
            _elements.push(e);
          }
        }
      }
      if (_elements.length === 0) {
        return "_dwb_no_hints_";
      }
      if (_markHints) {
        for (i=0; i<_elements.length; i++) {
          e = _elements[i];
          e.createOverlay();
          hints.appendChild(e.overlay);
        }
      }
      else {
        e = _elements[0];
        e.createOverlay();
        hints.appendChild(e.overlay);
      }
      document.body.appendChild(hints); 
      __setActive(_elements[0]);
      _activeArr = _elements;
    }
    catch (exc) {
      console.error(exc);
    }
    return null;
  };
  var __submitSearchEngine = function (string) {
    var e = __getActive().element;
    e.value = string;
    e.form.submit();
    e.value = "";
    __clear();
    if (e.form.method.toLowerCase() == 'post') {
      return e.name;
    }
    return null;
  };
  var __focusInput = function() {
    var i;
    var res = document.body.querySelectorAll('input[type=text], input[type=password], textarea');
    if (res.length === 0) {
      return "_dwb_no_input_";
    }
    var styles = document.styleSheets[0];
    styles.insertRule('input:focus { outline: 2px solid #1793d1; }', 0);
    if (!_activeInput) {
      _activeInput = res[0];
    }
    else {
      for (i=0; i<res.length; i++) {
        if (res[i] == _activeInput) {
          _activeInput = res[i+1] || res[0];
          break;
        }
      }
    }
    _activeInput.focus();
    return null;
  };
  var __init = function (letter_seq, font, style,
          fg_color, bg_color, active_color, normal_color, border,  opacity, markHints) {
        _letterSeq  = letter_seq;
        _font = font;
        _style =  style.toLowerCase();
        _fgColor    = fg_color;
        _bgColor    = bg_color;
        _activeColor = __hexToRgb(active_color);
        _normalColor = __hexToRgb(normal_color);
        _hintBorder = border;
        _hintOpacity = opacity;
        _markHints = markHints;
        _bigFont = Math.ceil(font.replace(/\D/g, "") * 1.25) + "px";
  };


  return {
    createStylesheet : function() {
      __createStyleSheet(document);
    },
    showHints : 
      function(type, new_tab) {
        return __showHints(type, new_tab);
      },
    updateHints :
      function (input, type) {
        return __updateHints(input, type);
      },
    clear : 
      function () {
        __clear();
      },
    followActive :
      function (type) {
        return __evaluate(__getActive().element, type);
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
        return __addSearchEngine();
      },
    submitSearchEngine :
      function (string) {
        return __submitSearchEngine(string);
      },
    focusInput : 
      function () {
        __focusInput();
      },
    init: 
      function (letter_seq, font, style,
          fg_color, bg_color, active_color, normal_color, border,  opacity, highlightLinks) {
        __init(letter_seq, font, style, fg_color, bg_color, active_color, normal_color, border,  opacity, highlightLinks); 
      }
  };
})();
