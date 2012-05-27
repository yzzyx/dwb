Object.freeze((function () {
  String.prototype.isInt = function () { 
    return !isNaN(parseInt(this, 10)); 
  };
  String.prototype.isLower = function () { 
    return this == this.toLowerCase(); 
  };
  var globals = {
    active : null,
    activeArr : [],
    activeInput : null,
    elements : [],
    positions : [],
    lastInput : null,
    lastPosition : 0,
    newTab : false,
    hintTypes :  [ "a, map, textarea, select, input:not([type=hidden]), button,  frame, iframe, [onclick], [onmousedown]," + 
        "[onmouseover], [role=link], [role=option], [role=button], [role=option], img",  // HINT_T_ALL
                   //[ "iframe", 
                   "a",  // HINT_T_LINKS
                   "img",  // HINT_T_IMAGES
                   "input[type=text], input[type=password], input[type=search], textarea",  // HINT_T_EDITABLE
                   "[src], [href]"  // HINT_T_URL
                 ]
  };
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
      l = globals.letterSeq.length;
      max = Math.ceil(Math.log(length)/Math.log(l));
      r = Math.ceil(Math.pow(length, 1/max));
      for (i=0; i<length; i++) {
        e = arr[i];
        text = new String();
        cur = i;
        for (j=0; j<max; j++) {
          text += globals.letterSeq[(cur%r)];
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
    for (var i=0; i<globals.positions.length; i++) {
      var p = globals.positions[i];
      if ((p.top -globals.fontSize <= t) && ( t <= p.top + globals.fontSize) && (l<=p.left + globals.fontSize) && (p.left-globals.fontSize <= l) ) {
        l+=Math.ceil(globals.fontSize*2.5);
        break;
      }
    }
    hint.style.top = t + "px";
    hint.style.marginTop = oe.marginTop;
    hint.style.left = l + "px";
    hint.style.marginLeft = oe.marginLeft;
    globals.positions.push({top : t, left : l});

    hint.className =  "dwb_hint";
    this.createOverlay = function() {
      if (this.element instanceof HTMLAreaElement) 
        return;
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
      var length = globals.activeArr.length;
      var i, cl;
      if (input.isInt()) {
        return 0;
      }
      var bestposition = 37;
      var ret = 0;
      for (i=0; i<length; i++) {
        var e = globals.activeArr[i];
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
      __getTextHints(globals.activeArr);
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
        globals.newTab & OpenMode.OPEN_NEW_VIEW ? 1 : 0, null);
    e.dispatchEvent(mouseEvent);
  };
  var __clickElement = function (element, ev) {
    var clicked = false;
    if (arguments.length == 2) {
      __mouseEvent(element, ev, !globals.newTab);
    }
    else {
      if (e.hasAttribute("onclick")) {
        __mouseEvent(element, ev, !globals.newTab);
        clicked = true;
      }
      if (e.hasAttribute("onmousedown")) {
        __mouseEvent(element, "mousedown", !globals.newTab);
        clicked = true;
      }
      if (e.hasAttribute("onmouseover")) {
        __mouseEvent(element, "mousedown", !globals.newTab);
        clicked = true;
      }
      if (!clicked) {
        __mouseEvent(element, "click", !globals.newTab);
        __mouseEvent(element, "mousedown", !globals.newTab);
        __mouseEvent(element, "mouseover", !globals.newTab);
      }
    }
  };
  var __getActive = function () {
    return globals.active;
  };
  var __setActive = function (element) {
    var active = __getActive();
    if (active) {
      if (globals.markHints) {
        active.overlay.style.background = globals.normalColor;
      }
      else if (active.overlay) {
        active.overlay.parentNode.removeChild(active.overlay);
      }
      active.hint.style.font = globals.font;

    }
    globals.active = element;
    if (!globals.active.overlay) {
      globals.active.createOverlay();
    }
    if (!globals.markHints) {
      globals.active.hint.parentNode.appendChild(globals.active.overlay);
    }
    globals.active.overlay.style.background = globals.activeColor;
    globals.active.hint.style.fontSize = globals.bigFont;
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
    return "rgba(" + rgb.slice(1) + "," +  globals.hintOpacity/2 + ")";
  };
  var __createStyleSheet = function(doc) {
    if (doc.hasStyleSheet) {
      return;
    }
    var styleSheet = __createElement("style");
    styleSheet.innerHTML += ".dwb_hint { " +
      "position:absolute; z-index:20000;" +
      "background:" + globals.bgColor  + ";" + 
      "color:" + globals.fgColor + ";" + 
      "border:" + globals.hintBorder + ";" + 
      "font:" + globals.font + ";" + 
      "display:inline;" +
      "opacity: " + globals.hintOpacity + "; }" + 
      ".dwb_overlay_normal { position:absolute!important;display:block!important; z-index:19999;background:" + globals.normalColor + ";}";
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
  var __appendHint = function (hints, varructor, e, win, r, oe) {
    var element = new varructor(e, win, r, oe);
    globals.elements.push(element);
    hints.appendChild(element.hint);
    if (globals.markHints) {
      element.createOverlay();
      hints.appendChild(element.overlay);
    }
  };
  var __createMap = function (hints, varructor, e, win, r, oe) {
    var map = null, a, i, coords, offsets;
    var mapid = e.getAttribute("usemap");
    var maps = win.document.getElementsByName(mapid.substring(1));
    for (i = 0; i<maps.length; i++) {
      if (maps[i] instanceof HTMLMapElement)  {
        map = maps[i]; 
        break;
      }
    }
    if (map === null)
      return;
    var areas = map.querySelectorAll("area");
    for (i=0; i<areas.length; i++) {
      a = areas[i];
      if (!a.hasAttribute("href"))
        continue;
      coords = a.coords.split(",", 2);
      offsets = { 
        offX : oe.offX + parseInt(coords[0], 10), 
        offY : oe.offY + parseInt(coords[1], 10), 
        marginTop : oe.marginTop, 
        marginLeft : oe.marginLeft
      };
      __appendHint(hints, varructor, a, win, r, offsets);
    }
  };
  var __createHints = function(win, varructor, type) {
    var i;
    try {
      var doc = win.document;
      var res = doc.body.querySelectorAll(globals.hintTypes[type]); 
      var e, r;
      __createStyleSheet(doc);
      var hints = doc.createDocumentFragment();
      var oe = __getOffsets(doc);
      for (i=0;i < res.length; i++) {
        e = res[i];
        if ((r = __getVisibility(e, win)) === null) {
          continue;
        }
        if ( (e instanceof HTMLFrameElement || e instanceof HTMLIFrameElement)) {
          __createHints(e.contentWindow, varructor, type);
          continue;
        }
        else if (e instanceof HTMLImageElement && e.hasAttribute("usemap")) {
          __createMap(hints, varructor, e, win, r, oe);
        }
        else {
          __appendHint(hints, varructor, e, win, r, oe);
        }
      }
      doc.body.appendChild(hints);
    }
    catch(exc) {
      console.error(exc);
    }
  };
  var __showHints = function (type, newTab) {
    var i;
    if (document.activeElement) {
      document.activeElement.blur();
    }
    globals.newTab = newTab;
    __createHints(window, globals.style == "letter" ? __letterHint : __numberHint, type);
    var l = globals.elements.length;

    if (l === 0) {
      return "_dwb_no_hints_";
    }
    else if (l == 1)  {
      return  __evaluate(globals.elements[0].element, type);
    }

    __getTextHints(globals.elements);
    globals.activeArr = globals.elements;
    __setActive(globals.elements[0]);
    return null;
  };
  var __updateHints = function(input, type) {
    var i;
    var array = [];
    var matchHint = false;
    if (!globals.activeArr.length) {
      __clear();
      __showHints(type, globals.newTab);
    }
    if (globals.lastInput && (globals.lastInput.length > input.length)) {
      __clear();
      globals.lastInput = input;
      __showHints(type, globals.newTab);
      return __updateHints(input, type);
    }
    globals.lastInput = input;
    if (input) {
      if (globals.style == "number") {
        if (input[input.length-1].isInt()) {
          input = input.match(/[0-9]+/g).join("");
          matchHint = true;
        }
        else {
          input = input.match(/[^0-9]+/g).join("");
        }
      }
      else if (globals.style == "letter") {
        var lowerSeq = globals.letterSeq.toLowerCase();
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
    for (i=0; i<globals.activeArr.length; i++) {
      var e = globals.activeArr[i];
      if (e.matchText(input, matchHint)) {
        array.push(e);
      }
      else {
        e.hint.style.visibility = 'hidden';
      }
    }
    globals.activeArr = array;
    if (array.length === 0) {
      __clear();
      return "_dwb_no_hints_";
    }
    else if (array.length == 1 && globals.autoFollow) {
      return __evaluate(array[0].element, type);
    }
    else {
      globals.lastPosition = array[0].betterMatch(input);
      __setActive(array[globals.lastPosition]);
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
      if (globals.elements) {
        for (i=0; i<globals.elements.length; i++) {
          var e = globals.elements[i];
          if ( (p = e.hint.parentNode) ) {
            p.removeChild(e.hint);
          }
          if (e.overlay && (p = e.overlay.parentNode)) {
            p.removeChild(e.overlay);
          }
        }
      }
      if(! globals.markHints && globals.active) {
        globals.active.element.removeAttribute("dwb_highlight");
      }
    }
    catch (exc) { 
      console.error(exc); 
    }
    globals.elements = [];
    globals.activeArr = [];
    globals.active = null;
    globals.lastPosition = 0;
    globals.lastInput = null;
    globals.positions = [];
  };
  var __evaluate = function (e, type) {
    var ret = null;
    var elementType = null;
    if (e.type) {
      elementType = e.type.toLowerCase();
    }
    var tagname = e.tagName.toLowerCase();
    if (globals.newTab && e.target == "_blank") {
      e.target = null;
    }
    if (type > 0) {
      switch (type) {
        case HintTypes.HINT_T_IMAGES: ret = e.src; break;
        case HintTypes.HINT_T_URL   : ret = e.hasAttribute("href") ? e.href : e.src; __clear(); return ret;
        default: break;
      }
    }
    if ((tagname && (tagname == "input" || tagname == "textarea"))) {
      if (type == "radio" || type == "checkbox") {
        e.focus();
        __clickElement(e, "click");
        ret = ret || "_dwb_check_";
      }
      else if (elementType && (elementType == "submit" || elementType == "reset" || elementType  == "button")) {
        __clickElement(e, "click");
        ret = ret || "_dwb_click_";
      }
      else {
        e.focus();
        ret = ret || "_dwb_input_";
      }
    }
    else if (e.hasAttribute("role")) {
      __clickElement(e);
      ret = ret || "_dwb_click_";
    }
    else {
      if (tagname == "a" || e.hasAttribute("onclick")) {
        __clickElement(e, "click");
      }
      else if (e.hasAttribute("onmousedown")) {
        __clickElement(e, "mousedown");
      }
      else if (e.hasAttribute("onmouseover")) {
        __clickElement(e, "mouseover");
      }
      else {
        __clickElement(e);
      }
      ret = ret || "_dwb_click_";
    }
    __clear();
    return ret;
  };
  var __focusNext = function()  {
    var newpos = globals.lastPosition === globals.activeArr.length-1 ? 0 : globals.lastPosition + 1;
    __setActive(globals.activeArr[newpos]);
    globals.lastPosition = newpos;
  };
  var __focusPrev = function()  {
    var newpos = globals.lastPosition === 0 ? globals.activeArr.length-1 : globals.lastPosition - 1;
    __setActive(globals.activeArr[newpos]);
    globals.lastPosition = newpos;
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
            globals.elements.push(e);
          }
        }
      }
      if (globals.elements.length === 0) {
        return "_dwb_no_hints_";
      }
      if (globals.markHints) {
        for (i=0; i<globals.elements.length; i++) {
          e = globals.elements[i];
          e.createOverlay();
          hints.appendChild(e.overlay);
        }
      }
      else {
        e = globals.elements[0];
        e.createOverlay();
        hints.appendChild(e.overlay);
      }
      document.body.appendChild(hints); 
      __setActive(globals.elements[0]);
      globals.activeArr = globals.elements;
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
    if (!globals.activeInput) {
      globals.activeInput = res[0];
    }
    else {
      for (i=0; i<res.length; i++) {
        if (res[i] == globals.activeInput) {
          globals.activeInput = res[i+1] || res[0];
          break;
        }
      }
    }
    globals.activeInput.focus();
    return null;
  };
  var __init = function (letter_seq, font, style,
          fg_color, bg_color, active_color, normal_color, border,  opacity, markHints, autoFollow) {
        globals.hintOpacity = opacity;
        globals.letterSeq  = letter_seq;
        globals.font = font;
        globals.style =  style.toLowerCase();
        globals.fgColor    = fg_color;
        globals.bgColor    = bg_color;
        globals.activeColor = __hexToRgb(active_color);
        globals.normalColor = __hexToRgb(normal_color);
        globals.hintBorder = border;
        globals.markHints = markHints;
        globals.autoFollow = autoFollow;
        globals.bigFont = Math.ceil(font.replace(/\D/g, "") * 1.25) + "px";
        globals.fontSize = Math.ceil(font.replace(/\D/g, ""))/2;
  };


  return {
    createStyleSheet : function() {
      __createStyleSheet(document);
    },
    showHints : 
      function(obj) {
        return __showHints(obj.type, obj.newTab);
      },
    updateHints :
      function (obj) {
        return __updateHints(obj.input, obj.type);
      },
    clear : 
      function () {
        __clear();
      },
    followActive :
      function (obj) {
        return __evaluate(__getActive().element, obj.type);
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
      function (obj) {
        return __submitSearchEngine(obj.searchString);
      },
    focusInput : 
      function () {
        __focusInput();
      },
    init : 
      function (obj) {
        __init(obj.hintLetterSeq, obj.hintFont, obj.hintStyle, obj.hintFgColor,
            obj.hintBgColor, obj.hintActiveColor, obj.hintNormalColor,
            obj.hintBorder, obj.hintOpacity, obj.hintHighlighLinks, obj.hintAutoFollow);
      }
  };
})());
