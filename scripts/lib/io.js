(function () {
  var prefixMessage     = "\n==> DEBUG [MESSAGE]    : ";
  var prefixError       = "\n==> DEBUG [ERROR]      : ";
  var prefixStack       = "\n==> DEBUG [STACK]      : ";
  var prefixArguments   = "\n==> DEBUG [ARGUMENTS]  : ";
  var prefixCaller      = "\n==> DEBUG [CALLER]";
  var prefixFunction = "\n------>";
  var regHasDwb = new RegExp("[^]*/\\*<dwb\\*/([^]*)/\\*dwb>\\*/[^]*");

  Object.defineProperties(io, {
    "debug" : {
      value : function (params) {
        var message = new String();
        params = params || {};
        var offset = params.offset || 0;
        if (params.message) {
          message += prefixMessage + params.message;
        }
        if (params.error && params.error instanceof Error) {
          var line = params.error.line || params.error.line === 0 ? params.error.line : "?";
          var error = params.error;
          if (!error.stack) {
            try {
              throw new Error(error.message);
            }
            catch(e) { 
              error = e;
            }
            offset += 1;
          }
          message += prefixError + "Exception in line " + line + ": " + error.message + 
            prefixStack + "[" + error.stack.match(/[^\n]+/g).slice(offset).join("] [")+"]"; 
        }
        else {
          try {
            throw new Error();
          }
          catch(e) {
            message += prefixStack + "[" + e.stack.match(/[^\n]+/g).slice(offset + 1).join("] [")+"]"; 
          }
        }
        if (params.arguments) 
        {
          message += prefixArguments + JSON.stringify(params.arguments);
          var caller = String(params.arguments.callee.caller);
          message += prefixCaller;
          message += prefixFunction + "\n";
          message += caller.replace(regHasDwb, "$1").replace(/\n/gm, "\n  ");
          message += prefixFunction;
        }
        io.print(message + "\n", "stderr");
      }

    }
  });
})();
Object.freeze(io);
