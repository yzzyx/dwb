String.prototype.isInt = function() { return !isNaN(parseInt(this)); }
const DwbHintObj = {
  _letterSeq : "FDSARTGBVECWXQYIOPMNHZULKJ",
  _fontSize : "12px",
  _fontWeight : "normal",
  _fontFamily : "monospace",
  _style : "letter",
  _fgColor : "#ffffff",
  _bgColor : "#000088",
  _activeColor : "#00ff00",
  _normalColor : "#ffff99",
  _hintBorder : "2px dashed #000000",
  _hintOpacity : 0.65,
  _elements : [], 
  _activeArr : [], 
  _lastInput : null,
  _lastPosition : 0,
  _activeInput : null,
  _hintTypes : 'a, img, textarea, select, link, input:not([type=hidden]), button,  frame, iframe, *[onclick], *[onmousedown], *[role=link]',


  createElement: 
    function(tagname) {
      element = document.createElement(tagname);
      if (!element.style) {
        var namespace = document.getElementsByTagName('html')[0].getAttribute('xmlns') || "http://www.w3.org/1999/xhtml";
        element = document.createElementNS(namespace, tagname);
      }
      return element;
    },
  hint : 
    function (element, win, offset, rect) {
      const me = DwbHintObj;
      this.element = element;
      this.win = win;
      if (!offset) {
        offset = [ 0, 0 ];
      }

      function create_span(element) {
        var span = me.createElement("span");
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
        hint.style.fontSize = me._fontSize;
        hint.style.fontWeight = me._fontWeight;
        hint.style.fontFamily = me._fontFamily;
        hint.style.color = me._fgColor;
        hint.style.background = me._bgColor;
        hint.style.opacity = me._hintOpacity;
        hint.style.border = me._hintBorder;
        hint.style.zIndex = 20000;
        hint.style.visibility = 'visible';
        hint.name = "dwb_hint";

        return hint;
      }

      this.hint = create_hint(element);
    },

  numberHint : 
    function (element, win, offset, rect) {
      const me = DwbHintObj;
      this.constructor = me.hint;
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
        for (var i=0; i<me._activeArr.length; i++) {
          var e = me._activeArr[i];
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
    },

  letterHint : 
    function (element, win, offset, rect) {
      const me = DwbHintObj;
      this.constructor = me.hint;
      this.constructor(element, win, offset, rect);

      this.betterMatch = function(input) {
        return 0;
      }
      this.getTextHint = function(i, length) {
        var text;
        var l = me._letterSeq.length;
        if (length < l) {
          text = me._letterSeq[i];
        }
        else if (length < 2*l) {
          var rem = (length) % l;
          var sqrt = Math.sqrt(2*rem);
          var r = sqrt == (getint = parseInt(sqrt)) ? sqrt + 1 : getint;
          if (i < l-r) {
            text = me._letterSeq[i];
          }
          else {
            var newrem = i%(r*r);
            text = me._letterSeq[Math.floor( (newrem / r) + l - r )] + me._letterSeq[l-newrem%r - 1];
          }
        }
        else {
          text = me._letterSeq[i%l] + me._letterSeq[l - 1 - (parseInt(i/l))];
        }
        var content = document.createTextNode(text);
        this.hint.appendChild(content);
      }
      this.matchText = function(input) {
        return (this.hint.textContent.toLowerCase().indexOf(input.toLowerCase()) == 0);
      }
    },

  mouseEvent :
    function (e, ev) {
      var mouseEvent = document.createEvent("MouseEvent");
      mouseEvent.initMouseEvent(ev, true, true, e.win, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
      e.dispatchEvent(mouseEvent);
    },
  clickElement : 
    function (element, ev) {
      const me = DwbHintObj;
      if (ev) {
        me.mouseEvent(element, ev);
      }
      else { // both events, if no event is given
        me.MouseEvent(element, "click");
        me.MouseEvent(element, "mousedown");
      }
      me.clear();
    },
  hexToRgb : 
    function (color) {
      const me = DwbHintObj;
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
      return "rgba(" + rgb.slice(1) + "," +  me._hintOpacity + ")";
    },
  createStyleSheet : 
    function () {
      const me = DwbHintObj;

      var style = document.styleSheets[document.styleSheets.length - 1];
      style.insertRule('*[dwb_highlight=hint_normal] { background-color: ' + me._normalColor + ' !important; } ', 0);
      style.insertRule('*[dwb_highlight=hint_active] { background-color: ' + me._activeColor + ' !important; } ', 0);
    },

  getVisibility :
    function (e) {
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
    },

  getElement : 
    function (win, e, offset, constructor) {
      const me = DwbHintObj;
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
          me._elements.push(element);
        }
      }
      if (r = me.getVisibility(e)) {
        if ( (e instanceof HTMLFrameElement || e instanceof HTMLIFrameElement)) {
          const doc = e.contentDocument ? e.contentDocument : e.contentWindow.document;
          if (doc) {
            var res = doc.body.querySelectorAll(me._hintTypes);
            var off = [ leftoff + e.offsetLeft, topoff + e.offsetTop ];
            for (var i=0; i < res.length; i++) {
              me.getElement(e, res[i], off, constructor);
            }
          }
        }
        else {
          var off = [ leftoff, topoff ];
          var element = new constructor(e, win, off, r);
          me._elements.push(element);
        }
      }
    },
  showHints : 
    function () {
      const me = DwbHintObj;
      if (document.activeElement) 
        document.activeElement.blur();

      me.createStyleSheet();

      var hints = me.createElement("div");
      hints.id = "dwb_hints";
      var constructor = me._style == "letter" ? me.letterHint : me.numberHint;

      var res = document.body.querySelectorAll(me._hintTypes);
      for (var i=0; i<res.length; i++) {
        me.getElement(window, res[i], null, constructor);
      };
      for (var i=0; i<me._elements.length; i++) {
        if (res[i] == me._elements[i]) {
          continue;
        }
        var e = me._elements[i];
        hints.appendChild(e.hint);
        e.getTextHint(i, me._elements.length);
        e.element.setAttribute('dwb_highlight', 'hint_normal');
        e.hint.style.visibility = 'visible';
      }
      me._activeArr = me._elements;
      me.setActive(me._elements[0]);
      document.body.appendChild(hints);
    }, 
  updateHints :
    function updateHints(input) {
      const me = DwbHintObj;
      var array = [];
      var text_content;
      var keep = false;
      if (!me._activeArr.length) {
        me.clear();
        me.showHints();
      }
      if (input) {
        input = input.toLowerCase();
      }
      if (me._lastInput && (me._lastInput.length > input.length)) {
        me.clear();
        me._lastInput = input;
        me.showHints();
        me.updateHints(input);
        return;
      }
      me._lastInput = input;
      if (me._style == "number" && input) {
        if (input[input.length-1].isInt()) {
          input = input.match(/[0-9]+/g).join("");
        }
        else {
          input = input.match(/[^0-9]+/g).join("");
        }
      }
      for (var i=0; i<me._activeArr.length; i++) {
        var e = me._activeArr[i];
        if (e.matchText(input)) {
          array.push(e);
        }
        else {
          e.hint.style.visibility = 'hidden';
          e.element.removeAttribute('dwb_highlight');
        }
      }
      me._activeArr = array;
      active = array[0];
      if (array.length == 0) {
        me.clear();
        return "_dwb_no_hints_";
      }
      else if (array.length == 1) {
        return me.evaluate(array[0].element);
      }
      else {
        me._lastPosition = array[0].betterMatch(input);
        me.setActive(array[me._lastPosition]);
      }
    },
  setActive : 
    function (element) {
      var active = DwbHintObj.getActive();
      if (active) {
        active.setAttribute('dwb_highlight', 'hint_normal' );
      }
      element.element.setAttribute('dwb_highlight', 'hint_active');
    },
  clear : 
    function () {
      const me = DwbHintObj;
      if (me._elements) {
        for (var i=0; i<me._elements.length; i++) {
          me._elements[i].element.removeAttribute('dwb_highlight');
        }
      }
      var hints = document.getElementById("dwb_hints");
      if (hints) 
        hints.parentNode.removeChild(hints);
      me._elements = [];
      me._activeArr = [];
    },
  evaluate :
    function (e) {
      const me = DwbHintObj;
      var ret, type;
      if (e.type) 
        type = e.type.toLowerCase();
      var tagname = e.tagName.toLowerCase();

      if (tagname && (tagname == "input" || tagname == "textarea") ) {
        if (type == "radio" || type == "checkbox") {
          e.focus();
          me.clickElement(e, "click");
          ret = "_dwb_check_";
        }
        else if (type == "submit" || type == "reset" || type  == "button") {
          me.clickElement(e, "click");
          ret = "_dwb_click_";
        }
        else {
          e.focus();
          ret = "_dwb_input_";
        }
      }
      else {
        if (tagname == "a" || e.hasAttribute("onclick")) 
          me.clickElement(e, "click");
        else if (e.hasAttribute("onmousedown")) 
          me.clickElement(e, "mousedown");
        else {
          me.clickElement(e);
        }
        ret = "_dwb_click_";
      }
      me.clear();
      return ret;
    },
  getActive :
    function () {
      return document.querySelector('*[dwb_highlight=hint_active]');
    },
  followActive :
    function () {
      return DwbHintObj.evaluate(DwbHintObj.getActive());
    },

  focusNext :
    function () {
      const me = DwbHintObj;
      var newpos = me._lastPosition == me._activeArr.length-1 ? 0 : me._lastPosition + 1;
      active = me._activeArr[newpos];
      me.setActive(active);
      me._lastPosition = newpos;
    },
  focusPrev : 
    function () {
      const me = DwbHintObj;
      var newpos = me._lastPosition == 0 ? me._activeArr.length-1 : me._lastPosition - 1;
      active = me._activeArr[newpos];
      me.setActive(active);
      me._lastPosition = newpos;
    },
  addSearchEngine : 
    function () {
      const me = DwbHintObj;
      me.createStyleSheet();
      var hints = me.createElement("div");
      var res = document.body.querySelectorAll("form");

      for (var i=0; i<res.length; i++) {
        var els = res[i].elements;
        for (var j=0; j<els.length; j++) {
          if (me.getVisibility(els[j]) && (els[j].type == "text" || els[j].type == "search")) {
            var e = new me.letterHint(els[j], window);
            me._elements.push(e);
            e.element.setAttribute('dwb_highlight', 'hint_normal');
          }
        }
      }
      if (!me._elements.length) {
        return "_dwb_no_hints_";
      }
      for (var i=0; i<me._elements.length; i++) {
        me._elements[i].getTextHint(i, me._elements.length);
        me._elements[i].element.setAttribute('dwb_highlight', 'hint_normal');
      }
      document.body.appendChild(hints); 
      me.setActive(me._elements[0]);
      me._activeArr = me._elements;
    },
  submitSearchEngine :
    function (string) {
      var e = DwbHintObj.getActive();
      e.value = string;
      e.form.submit();
      e.value = "";
      if (e.form.method.toLowerCase() == 'post') {
        return e.name;
      }
      return NULL;
    },
  focusInput : 
    function dwb_focus_input() {
      const me = DwbHintObj;
      var res = document.body.querySelectorAll('input[type=text], input[type=password], textarea');
      if (res.length == 0) {
        return "_dwb_no_input_";
      }
      styles = document.styleSheets[0];
      styles.insertRule('input:focus { outline: 2px solid #1793d1; }', 0);
      if (!me._activeInput) {
        me._activeInput = res[0];
      }
      else {
        for (var i=0; i<res.length; i++) {
          if (res[i] == me._activeInput) {
            if (!(me._activeInput = res[i+1])) {
              me._activeInput = res[0];
            }
            break;
          }
        }
      }
      me._activeInput.focus();
    },
  init: 
    function (letter_seq, font_size, font_weight, font_family, style,
        fg_color, bg_color, active_color, normal_color, border,  opacity) {
      const me = DwbHintObj;
      me._letterSeq  = letter_seq;
      me._fontSize = font_size;
      me._fontWeight = font_weight;
      me._fontFamily = font_family;
      me._style =  style.toLowerCase();
      me._fgColor    = fg_color;
      me._bgColor    = bg_color;
      me._activeColor = me.hexToRgb(active_color);
      me._normalColor = me.hexToRgb(normal_color);
      me._hintBorder = border;
      me._hintOpacity = opacity;
    }, 
}
