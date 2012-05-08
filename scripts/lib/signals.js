(function ()  {
  var _registered = {};
  Object.defineProperties(signals, {
    "emit" : {
      value : function(sig, args) {
        var sigs = _registered[sig];
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
        if (_registered[sig].length === 0) {
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
          if (_registered[sig] === undefined || _registered[sig] === null) {
            _registered[sig] = [];
            signals[sig] = function () { return signals.emit(sig, arguments); };
          }
          if (func === null || typeof func !== "function") {
            return -1;
          }
          _registered[sig].push({callback : func, id : id, connected : true });
          return id;
        };
      })()
    },
    "disconnect" : {
      value : function(id) {
        var sig, i, sigs;
        for (sig in _registered) {
          sigs = _registered[sig];
          for (i = 0; i<sigs.length; i++) {
            if (sigs[i].id == id) {
              if (_registered[sig].length === 1) {
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
        for (sig in _registered) {
          sigs = _registered[sig];
          for (i = 0; i<sigs.length; i++) {
            if (sigs[i].callback == func) {
              if (_registered[sig].length === 1) {
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
        io.print(name);
        if (signals[name] !== null && signals[name] !== undefined) {
          signals[name] = null;
          return true;
        }
        return false;
      }
    }
  });
})();
