Object.defineProperties(signals, {
  "_registered" : {
    value : {}
  },
  "emit" : {
    value : function(sig, args) {
      var sigs = signals._registered[sig];
      var ret = false;
      var i = 0;
      do {
        if (sigs[i].connected) {
          ret = sigs[i].callback.apply(this, args) || ret;
          i++;
        }
        else {
          sigs.splice(i, 1);
        }
      } while (i<sigs.length);
      if (signals._registered[sig].length === 0) {
        signals[sig] = null;
      }
      return ret;
    }
  },
  "connect" : {
    value : (function () {
      var id = 0;
      return function(sig, func) {
        ++id;
        if (signals._registered[sig] === undefined || signals._registered[sig] === null) {
          signals._registered[sig] = [];
          signals[sig] = function () { return signals.emit(sig, arguments); };
        }
        if (func === null || typeof func !== "function") {
          return -1;
        }
        signals._registered[sig].push({callback : func, id : id, connected : true });
        return id;
      };
    })()
  },
  "disconnect" : {
    value : function(id) {
      var sig, i, sigs;
      for (sig in signals._registered) {
        sigs = signals._registered[sig];
        for (i = 0; i<sigs.length; i++) {
          if (sigs[i].id == id) {
            if (signals._registered[sig].length === 1) {
              signals[sig] = null;
            }
            else {
              sigs[i].connected = false;
            }
            return true;
          }
        }
      }
      return false;
    }
  },
  "disconnectByFunction" : {
    value : function(func) {
      var sig, i, sigs, ret = false;
      for (sig in signals._registered) {
        sigs = signals._registered[sig];
        for (i = 0; i<sigs.length; i++) {
          if (sigs[i].callback == func) {
            if (signals._registered[sig].length === 1) {
              signals[sig] = null;
            }
            else {
              sigs[i].connected = false;
            }
            ret = true;
          }
        }
      }
      return ret;
    }
  }, 
  "disconnectByName" : {
    value : function (name) {
      if (signals[name] !== null && signals[name] !== undefined) {
        signals[name] = null;
        return true;
      }
      return false;
    }
  }
});
