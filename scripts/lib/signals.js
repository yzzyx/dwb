
Object.defineProperty(signals, "registered", {
  value : {}
});
Object.defineProperty(signals, "emit", {
  value : function(sig, wv, o) {
    var sigs = signals.registered[sig];
    for (var i=0; i<sigs.length; i++) {
      sigs[i](wv, o);
    }
  }
});
Object.defineProperty(signals, "connect", {
  value : function(sig, func) {
    var connected = signals.registered[sig] !== undefined && signals.registered[sig] !== null;
    if (!connected)
      signals.registered[sig] = [];
    signals.registered[sig].push(func);
    if (!connected) {
      signals[sig] = function (wv, o) {
        signals.emit(sig, wv, o);
      };
    }
  }
});
Object.defineProperty(signals, "disconnect", {
  value : function(sig, func) {
    var sigs = signals.registered[sig];
    if (sigs) 
      delete sigs.splice(sigs.indexOf(func), 1);
  }
});
