(function () {
  var prefixMessage = "\n==> DEBUG [MESSAGE]  : ";
  var prefixError   = "\n==> DEBUG [ERROR]    : ";
  var prefixStack   = "\n==> DEBUG [STACK]    : ";
  Object.defineProperties(io, {
    "debug" : {
      value : function () {
        var message = null;
        if (arguments.length === 0) {
          return;
        }
        else if (arguments.length === 1) {
          if (arguments[0] instanceof Error) {
            message = prefixError + "Exception in line " + (arguments[0].line + 1) + ": " + arguments[0].message + 
                      prefixStack + "[" + arguments[0].stack.match(/[^\n]+/g).slice(0).join("] [")+"]"; 
          }
          else {
            try { 
              throw new Error(arguments[0]); 
            }
            catch (e) {
              message = prefixMessage + e.message + 
                        prefixStack + "[" + e.stack.match(/[^\n]+/g).slice(1).join("] [")+"]"; 
            }
          }
        }
        else {
          message = prefixMessage + arguments[0];
          if (arguments[1] instanceof Error) {
            message += prefixError + "Error in line " + (arguments[1].line + 1) + ": " + arguments[1].message;
            message += prefixStack + "[" + arguments[1].stack.match(/[^\n]+/g).slice(0).join("] [")+"]"; 
          }
        }
        io.print(message + "\n", "stderr");
      }

    }
  });
})();
Object.freeze(io);
