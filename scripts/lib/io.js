Object.defineProperties(io, {
  "debug" : {
    value : function () {
      var message = null;
      if (arguments.length === 0) {
        return;
      }
      else if (arguments.length === 1) {
        if (arguments[0] instanceof Error) {
          message = "\nSCRIPT DEBUG: \tException in line " + (arguments[0].line + 1) + ": " + arguments[0].message + "\n" + 
            "STACK: \t\t[" + arguments[0].stack.match(/[^\n]+/g).slice(0).join("] [")+"]\n"; 
        }
        else {
          try { 
            throw new Error(arguments[0]); 
          }
          catch (e) {
            message = "\nSCRIPT DEBUG: \t" + e.message + "\n" + 
                "STACK: \t\t[" + e.stack.match(/[^\n]+/g).slice(1).join("] [")+"]\n"; 
          }
        }
      }
      else {
        message = "\nSCRIPT DEBUG: \t" + arguments[0] + "\n";
        if (arguments[1] instanceof Error) {
          message += "SCRIPT DEBUG: \tException in line " + (arguments[1].line + 1) + ": " + arguments[1].message + "\n";
          message += "STACK: \t\t[" + arguments[1].stack.match(/[^\n]+/g).slice(0).join("] [")+"]\n"; 
        }
      }
      io.print(message, "stderr");
    }
  }
});
Object.freeze(io);
