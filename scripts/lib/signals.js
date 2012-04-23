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
    var i;
    for (i=0; i<sigs.length; i++) {
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
Object.defineProperty(signals, "disconnect", {
  value : function(id) {
    var name, i, sigs;
    for (name in signals._registered) {
      sigs = signals._registered[name];
      for (i = 0; i<sigs.length; i++) {
        if (sigs[i].id == id) {
          delete sigs.splice(i, 1);
          if (signals._registered[name].length === 0) 
            signals[name] = null;
          return;
        }
      }
    }
  }
});
