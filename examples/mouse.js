const SCROLL_ICON="transparent url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAHUwAADqYAAAOpgAABdwnLpRPAAAAi5QTFRFAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQEAAAEAAAAAAAAAAAAAAAAAAAAAAAAA////PMNlIQAAAKR0Uk5TAAAAD2p/JQqN+74iBn34sRkDbvT+ohIBX+78kwxQ5/mEB0Pf9XMEM9XwYgCLxhBc1tvc9f7Uu756BgQZHSXC/WIFCQMJu/1cCbv9XAm7/VwJu/1eB7f+YQa1/mEGtf5hBrX+YQgpLzTC/m4RFAhm5+vr+f/i0NOQCILHECPD6lUw0PBmAj3a9nUESuP6hQgAWOv8lA0CZ/H/pBIFdvWxGghQZRvM0CN6AAAAAWJLR0S5OrgWYAAAAAlwSFlzAAALEwAACxMBAJqcGAAAAUJJREFUOMuV0FNzREEQhuET27ZtOxvbtm3btm10bOfnBV0bnJ3Zqv1u3+diphlGsImIionz6xKSUiAtQ++ycvIAJwqKtK6krAJweqaqpk7MQhqaWgBwfnGpraNLAsJ6+p8drq5vbg0MjQjA2AS+d3f/8GhqZs7TLSyBu6fnFytrGzaw/enw+vYOdvZs4ODo5OyCwNXN3cPTiw28fXz9/BFwAgKDgkn/CAlFEBZOOVREJIKoaAqIiUUQF08BCYkIkpIpICUVQVo6BWRkIsjKpoCcXAR5+RRQUIigqJhUS0rLyisQVFZV19TygLr6hsYmBM0trW3tHWzQCf/W1c0GPb1/e18/7yMGBn/70PAI4ZmjY9w+PjFJ/ObUNPaZ2TnyHeYXFr/60vIK5VDM6to6wMbmFkPd9s4u7O0zfHZweHTMCLYPkhas1aCqNgQAAAAldEVYdGRhdGU6Y3JlYXRlADIwMTEtMDQtMDdUMTE6Mjk6MzcrMDI6MDB8AZWGAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDExLTA0LTA3VDExOjI5OjM3KzAyOjAwDVwtOgAAAABJRU5ErkJggg==) no-repeat scroll center";
// mouse functions
var mouse_init;
function MouseGesturesObj() {
  const BUTTON1 = 1<<0;
  const BUTTON2 = 1<<1;
  const BUTTON3 = 1<<2;
  const gap = 30;
  const min = Math.min(50, window.outerWidth / 2);
  const MouseGestures = {
    x : 0,
    y : 0,
    state : -1,
    target : null,
    span : null,
    timer_id : 0,
    addEventListener: 
      function(type, func, capture) { window.addEventListener(type, func, capture); },
    removeEventListener: 
      function(type, func, capture) { window.removeEventListener(type, func, capture); },
    remove_span: 
      function() {
        MouseGestures.span.parentNode.removeChild(MouseGestures.span);
        MouseGestures.timer_id = 0;
      },
    mouseDown: 
      function (ev) {
        if (!(MouseGestures.target = ev.target.href)) {
          MouseGestures.target = null;
        }
        if (ev.button == 1) {
          MouseGestures.state = 1;
          MouseGestures.x = ev.x;
          MouseGestures.y = ev.y;
          window.addEventListener('mousemove',   MouseGestures.mouseMove, false);
        }
      },
    clear: 
      function() {
        MouseGestures.target = null;
        MouseGestures.state = -1;
        window.removeEventListener('mousemove', MouseGestures.mouseMove, false);
      },
    mouseUp:
      function (ev) {
        if (ev.button != 1) {
          MouseGestures.clear();
          return;
        }
        if (Math.abs(ev.y - MouseGestures.y) < gap ) {
          if (MouseGestures.x - ev.x > min) {
            window.history.back();
          }
          else if (ev.x - MouseGestures.x > min)
            window.history.forward();
        }
        else if (Math.abs(ev.x - MouseGestures.x) < gap) {
          if (MouseGestures.y - ev.y > min) {
            window.open(MouseGestures.target);
          }
          else if (ev.y - MouseGestures.y > min)
            window.location.reload();
        }
        MouseGestures.clear();
      },
    mouseMove: 
      function (ev) {
        if (Math.abs(ev.y - MouseGestures.y) > gap && Math.abs(ev.x - MouseGestures.x) > gap) {
          MouseGestures.clear();
        }
      },
    init: 
      function () {
        window.addEventListener('mousedown', MouseGestures.mouseDown, false);
        window.addEventListener('mouseup',   MouseGestures.mouseUp, false);
      },

  }
  MouseGestures.init();
}
function MouseAutoScrollObj() {
  const SIZE = 32;
  const OFFSET = 5;
  var doc;
  Math.sign = function(x) { return x >=0 ? 1  : -1; }
  const MouseAutoScroll = {
    _span : null,
    _x : 0,
    _y : 0,
    _ev : null,
    _scrollX : 0,
    _scrollY : 0,
    _timerId : 0,
    _cursorStyle : null,
    startTimer : 
      function () {
        MouseAutoScroll._timerId = window.setInterval(MouseAutoScroll.timer, 40);
      },
    stopTimer : 
      function () {
        window.clearInterval(MouseAutoScroll._timerId);
        MouseAutoScroll._timerId = 0;
      },

    timer : 
      function () {
        if (MouseAutoScroll._ev) {
          var scrollY = (MouseAutoScroll._ev.y - MouseAutoScroll._y);
          var scrollX = (MouseAutoScroll._ev.x - MouseAutoScroll._x);
          var b = scrollY > 0 
            && doc.documentElement.scrollHeight == (window.innerHeight + window.pageYOffset);
          var r = scrollX > 0 
            && doc.documentElement.scrollWidth == (window.innerWidth + window.pageXOffset);
          var l = scrollX < 0 && window.pageXOffset == 0;
          var t = scrollY < 0 && window.pageYOffset == 0;
          var offX = Math.abs(scrollX) < OFFSET;
          var offY = Math.abs(scrollY) < OFFSET;
          if ( MouseAutoScroll._timerId != 0 
            && (( b && r ) || ( b && l) || ( b && offX ) 
            || ( t && r ) || ( t && l) || ( t && offX ) 
            || (offY && r) || (offY && l) )) {
            MouseAutoScroll.stopTimer();
            return;
          }
          window.scrollBy(scrollX - Math.sign(scrollX) * OFFSET, 
              scrollY - Math.sign(scrollY) * OFFSET);
        }
      },
    mouseMove : 
      function(ev) { 
        if (MouseAutoScroll._timerId == 0) {
          MouseAutoScroll.startTimer();
        }
        MouseAutoScroll._ev = ev; 
      },
    init : 
      function (ev) {
        doc = ev.target.ownerDocument;
        if (window.innerHeight >= doc.documentElement.scrollHeight 
            && window.innerWidth >= doc.documentElement.scrollWidth) {
          return;
        }
        var span = doc.createElement("div");
        span.style.width = SIZE + "px";
        span.style.height = SIZE + "px";
        span.style.background = SCROLL_ICON;
        span.style.left = ev.x - (SIZE / 2) + "px"; 
        span.style.top = ev.y - (SIZE / 2) + "px"; 
        span.style.position = "fixed";
        span.style.fontSize = SIZE + "px";
        span.style.opacity = 0.6;
        MouseAutoScroll._cursorStyle = doc.defaultView.getComputedStyle(doc.body, null).cursor;
        doc.body.style.cursor = "move";
        doc.body.appendChild(span);
        doc.addEventListener('mousemove', MouseAutoScroll.mouseMove, false);
        MouseAutoScroll._span = span;
      },
    clear : 
      function(ev) {
        doc.body.style.cursor = MouseAutoScroll._cursorStyle;
        MouseAutoScroll._span.parentNode.removeChild(MouseAutoScroll._span);
        doc.removeEventListener('mousemove', MouseAutoScroll.mouseMove, false);
        MouseAutoScroll.stopTimer();
        if (MouseAutoScroll._span)
          MouseAutoScroll._span =  null;
        if (MouseAutoScroll._ev)
          MouseAutoScroll._ev = null;
      },
    mouseUp : // Simulate click, click event does not work during scrolling
      function(ev) {
        if (Math.abs(ev.x - MouseAutoScroll._x) < 5 && Math.abs(ev.y - MouseAutoScroll._y) < 5) {
          MouseAutoScroll.init(ev);
          window.removeEventListener('mouseup', MouseAutoScroll.mouseUp, false);
        }
      },
    mouseDown : 
      function(ev) {
        var t = ev.target;
        console.log(t.tagName);
        if (ev.button == 1) {
          if (MouseAutoScroll._span) {
            MouseAutoScroll.clear();
          }
          else if (!t.hasAttribute("href") 
              && ! (t.tagName == "INPUT") 
              && ! (t.tagName == "TEXTAREA") 
              && !t.hasAttribute("onmousedown") 
              && !(t.hasAttribute("onclick"))) {
            MouseAutoScroll._x = ev.x;
            MouseAutoScroll._y = ev.y;
            window.addEventListener('mouseup', MouseAutoScroll.mouseUp, false);
          }
        }
      },
  }
  window.addEventListener('mousedown', MouseAutoScroll.mouseDown, false);
}
function MouseInit(gestures, autoscroll) {
  if (!mouse_init) {
    if (gestures) 
      new MouseGesturesObj();
    if (autoscroll)
      new MouseAutoScrollObj();
    mouse_init = 1;
  }
}
MouseInit(1, 1);
