MouseAutoScroll = (function() {
  const SCROLL_ICON="transparent url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAHUwAADqYAAAOpgAABdwnLpRPAAAAi5QTFRFAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQEAAAEAAAAAAAAAAAAAAAAAAAAAAAAA////PMNlIQAAAKR0Uk5TAAAAD2p/JQqN+74iBn34sRkDbvT+ohIBX+78kwxQ5/mEB0Pf9XMEM9XwYgCLxhBc1tvc9f7Uu756BgQZHSXC/WIFCQMJu/1cCbv9XAm7/VwJu/1eB7f+YQa1/mEGtf5hBrX+YQgpLzTC/m4RFAhm5+vr+f/i0NOQCILHECPD6lUw0PBmAj3a9nUESuP6hQgAWOv8lA0CZ/H/pBIFdvWxGghQZRvM0CN6AAAAAWJLR0S5OrgWYAAAAAlwSFlzAAALEwAACxMBAJqcGAAAAUJJREFUOMuV0FNzREEQhuET27ZtOxvbtm3btm10bOfnBV0bnJ3Zqv1u3+diphlGsImIionz6xKSUiAtQ++ycvIAJwqKtK6krAJweqaqpk7MQhqaWgBwfnGpraNLAsJ6+p8drq5vbg0MjQjA2AS+d3f/8GhqZs7TLSyBu6fnFytrGzaw/enw+vYOdvZs4ODo5OyCwNXN3cPTiw28fXz9/BFwAgKDgkn/CAlFEBZOOVREJIKoaAqIiUUQF08BCYkIkpIpICUVQVo6BWRkIsjKpoCcXAR5+RRQUIigqJhUS0rLyisQVFZV19TygLr6hsYmBM0trW3tHWzQCf/W1c0GPb1/e18/7yMGBn/70PAI4ZmjY9w+PjFJ/ObUNPaZ2TnyHeYXFr/60vIK5VDM6to6wMbmFkPd9s4u7O0zfHZweHTMCLYPkhas1aCqNgQAAAAldEVYdGRhdGU6Y3JlYXRlADIwMTEtMDQtMDdUMTE6Mjk6MzcrMDI6MDB8AZWGAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDExLTA0LTA3VDExOjI5OjM3KzAyOjAwDVwtOgAAAABJRU5ErkJggg==) no-repeat scroll center";
  const SIZE = 32;
  const OFFSET = 5;
  var doc;
  Math.sign = function(x) { return x >=0 ? 1  : -1; };
  _span = null;
  _x = 0;
  _y = 0;
  _ev = null;
  _scrollX = 0;
  _scrollY = 0;
  _timerId = 0;
  _cursorStyle = null;
  function startTimer() {
    _timerId = window.setInterval(timer, 40);
  }
  function stopTimer () {
    window.clearInterval(_timerId);
    _timerId = 0;
  }
  function timer () {
    if (_ev) {
      var scrollY = (_ev.y - _y);
      var scrollX = (_ev.x - _x);
      var b = scrollY > 0 
        && doc.documentElement.scrollHeight == (window.innerHeight + window.pageYOffset);
      var r = scrollX > 0 
        && doc.documentElement.scrollWidth == (window.innerWidth + window.pageXOffset);
      var l = scrollX < 0 && window.pageXOffset == 0;
      var t = scrollY < 0 && window.pageYOffset == 0;
      var offX = Math.abs(scrollX) < OFFSET;
      var offY = Math.abs(scrollY) < OFFSET;
      if ( _timerId != 0 
          && (( b && r ) || ( b && l) || ( b && offX ) 
            || ( t && r ) || ( t && l) || ( t && offX ) 
            || (offY && r) || (offY && l) )) {
              stopTimer();
              return;
            }
      window.scrollBy(scrollX - Math.sign(scrollX) * OFFSET, 
          scrollY - Math.sign(scrollY) * OFFSET);
    }
  }
  function mouseMove (ev) { 
    if (_timerId == 0) {
      startTimer();
    }
    _ev = ev; 
  }
  function init (ev) {
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
    _cursorStyle = doc.defaultView.getComputedStyle(doc.body, null).cursor;
    doc.body.style.cursor = "move";
    doc.body.appendChild(span);
    doc.addEventListener('mousemove', mouseMove, false);
    _span = span;
  }
  function clear (ev) {
    doc.body.style.cursor = _cursorStyle;
    _span.parentNode.removeChild(_span);
    doc.removeEventListener('mousemove', mouseMove, false);
    stopTimer();
    if (_span)
      _span =  null;
    if (_ev)
      _ev = null;
  }
  function mouseUp (ev) { // Simulate click, click event does not work during scrolling
    if (Math.abs(ev.x - _x) < 5 && Math.abs(ev.y - _y) < 5) {
      init(ev);
      window.removeEventListener('mouseup', mouseUp, false);
    }
  }
  function mouseDown (ev) {
    var t = ev.target;
    if (ev.button == 1) {
      if (_span) {
        clear();
      }
      else if (!t.hasAttribute("href") 
          && !t.hasAttribute("onmousedown") 
          && !(t.hasAttribute("onclick"))) {
        _x = ev.x;
        _y = ev.y;
        window.addEventListener('mouseup', mouseUp, false);
      }
    }
  }
  window.addEventListener('mousedown', mouseDown, false);
})();
