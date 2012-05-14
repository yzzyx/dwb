(function () {
  var _config = undefined;
  var _debug = false;
  function getPlugin(name, filename) {
    var ret = null;
    try {
      if (system.fileTest(filename, FileTest.exists)) {
        ret = include(filename);
      }
    }
    catch(e) {
      extensions.error(name, "Error in line " + e.line + " parsing " + filename);
    }
    return ret;
  }
  function getStack(offset) {
    if (arguments.length === 0) {
      offset = 0;
    }
    try {
      throw Error (message);
    }
    catch (e) {
      var stack = e.stack.match(/[^\n]+/g);
      return "STACK: [" + stack.slice(offset+1).join("] [")+"]";
    }
  }
  Object.defineProperties(extensions, { 
    "warning" : {
      value : function (name, message) {
        io.print("\033[1mDWB EXTENSION WARNING: \033[0mextension  \033[1m" + name + "\033[0m: " + message, "stderr");
      }
    }, 
    "enableDebugging" : {
      set : function (value) {
        if (typeof value == "boolean")
          _debug = value;
      }
    },
    "debug" : {
      value : function (name, message) {
        if (_debug) {
          io.print("\033[1mDWB EXTENSION DEBUG: \033[0mextension  \033[1m" + name + "\033[0m\n" + getStack(1), "stderr");
        }
      }
    }, 
    "error" : {
      value : function (name, a, b) {
        var message = "";
        if (a instanceof Error) {
          if (a.message) {
            message = a.message;
          }
          else if (arguments.length > 2)
            message = b;
          else 
            b = "";
          io.print("\033[31mDWB EXTENSION ERROR: \033[0mextension \033[1m" + name + "\033[0m in line " + a.line + ": " + 
              message + "\nSTACK: [" + a.stack.match(/[^\n]+/g).join("] [") + "]", "stderr");
        }
        else {
          io.print("\033[31mDWB EXTENSION ERROR: \033[0mextension \033[1m" + name + "\033[0m: " + a + "\n" + getStack(1), "stderr");
        }
      }
    },
    "message" : {
      value : function (name, message) {
        io.print("\033[1mDWB EXTENSION: \033[0mextension \033[1m" + name + "\033[0m: " + message, "stderr");
      }
    },
    "load" : {
      value : function(name, c) {
        var boldname = "\033[1m" + name + "\033[0m";

        var config, dataBase, pluginPath, plugin = null;
        var extConfig = null;

        /* Get default config if the config hasn't been read yet */
        if (arguments.length == 2) {
          extConfig = c;
        }
        else {
          if (_config === undefined) {
            try {
              config = include(data.configDir + "/extensionrc");
            }
            catch (e) {
              extensions.error(name, "loading config failed : " + e);
            }
            if (config === null) {
              extensions.warning(name, "Could not load config.");
            }
            else {
              _config = config;
            }
          }
          if (_config) {
            extConfig = _config[name] || null;
          }
        }

        /* Load extension */
        var filename = data.userDataDir + "/extensions/" + name;
        plugin = getPlugin(name, data.userDataDir + "/extensions/" + name);
        if (plugin === null) {
          plugin = getPlugin(name, data.systemDataDir + "/extensions/" + name);
          if (plugin === null) {
            extensions.error(name, "Couldn't find extension.");
            return false;
          }
        }
        try {
          if (plugin.init(extConfig)) {
            extensions.message(name, "Successfully loaded and initialized.");
            return true;
          }
          else {
            extensions.error(name, "Initialization failed.");
            return false;
          }
        }
        catch (e) {
          extensions.error(name, "Initialization failed: " + e);
          return false;
        }
      }
    }
  });
})();
Object.freeze(extensions);
