Object.defineProperty(signals, "_registered", {
  value : {}
});
Object.defineProperty(signals, "_maxId", {
  value : 0,
  writable : true
});
Object.defineProperty(signals, "emit", {
  value : function(sig, wv, o) {
    var sigs = signals._registered[sig];
    var ret = false;
    for (var i=0; i<sigs.length; i++) {
      ret = sigs[i].callback(wv, o) || ret;
    }
    return ret;
  }
});
Object.defineProperty(signals, "connect", {
  value : function(sig, func) {
    if (signals._registered[sig] === undefined || signals._registered[sig] === null) {
      signals._registered[sig] = [];
      signals[sig] = function (wv, o) {
        return signals.emit(sig, wv, o);
      };
    }
    if (func !== null && typeof func === "function") {
      signals._maxId++;
      signals._registered[sig].push({callback : func, id : signals._maxId });
      return signals._maxId;
    }
    return -1;
  }
});
Object.defineProperty(signals, "register", {
  value : function (sig) {
    signals.connect(sig, null);
  }
});
Object.defineProperty(signals, "disconnect", {
  value : function(id) {
    var s, i, a;
    for (s in signals._registered) {
      a = signals._registered[s];
      for (i = 0; i<a.length; i++) {
        if (a[i].id == id) {
          delete a.splice(i, 1);
        }
      }
    }
  }
});
