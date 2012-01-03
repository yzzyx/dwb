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
            window.scrollTo(0, 0);
          }
          else if (ev.y - MouseGestures.y > min)
            window.scrollTo(0, document.height - window.innerHeight);
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
new MouseGesturesObj();
