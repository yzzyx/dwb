(function () {
  var _config = {};
  var _debug = false;
  var _registered = {};
  var _configLoaded = false;
  var getPlugin = function(name, filename) {
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
  };
  var getStack = function(offset) {
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
  };
  var _unload = function (name, removeConfig) {
    if (_registered[name] !== undefined) {
      if (_registered[name].end instanceof Function) {
        _registered[name].end();
        extensions.message(name, "Extension unloaded.");
      }
      if (removeConfig)
        delete _config[name];
      delete _registered[name];
      return true;
    }
    return false;
  };
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
    "getConfig" : {
      value : function(c, dc) {
        var k, config = {};
        if (c === null || c === undefined)
          config =  dc;
        else {
          for (k in dc) {
            config[k] = typeof c[k] === typeof dc[k] ? c[k] : dc[k];
          }
        }
        return config;
      }
    }, 
    "load" : {
      value : function(name, c) {
        if (_registered[name] !== undefined) {
            extensions.error(name, "Already loaded.");
        }
        var boldname = "\033[1m" + name + "\033[0m";

        var config, dataBase, pluginPath, plugin = null, key, filename;
        var extConfig = null;

        /* Get default config if the config hasn't been read yet */
        if (arguments.length == 2) {
          extConfig = c;
          _config[name] = c;
        }
        if (!_configLoaded) {
          if (system.fileTest(data.configDir + "/extensionrc", FileTest.regular)) {
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
              for (key in config) {
                _config[key] = config[key];
              }
            }
            _configLoaded = true;
          }
        }
        if (extConfig === null) 
          extConfig = _config[name] || null;

        /* Load extension */
        if (data.userDataDir) {
          filename = data.userDataDir + "/extensions/" + name;
          plugin = getPlugin(name, data.userDataDir + "/extensions/" + name);
        }
        if (plugin === null) {
          plugin = getPlugin(name, data.systemDataDir + "/extensions/" + name);
          if (plugin === null) {
            extensions.error(name, "Couldn't find extension.");
            return false;
          }
        }
        try {
          plugin._name = name;
          if (plugin.init(extConfig)) {
            _registered[name] = plugin;
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
    },
    "unload" : { 
      value : function(name) {
        return _unload(name, true);
      }
    }, 
    "reload" : {
      value : function () {
        _unload(arguments[0], false);
        return extensions.load.apply(this, arguments);
      }
    }, 
    "toggle" : {
      value : function(name, c) {
        if (_registered[name] !== undefined) {
          _unload(name);
          return false;
        }
        else {
          extensions.load(name, c);
          return true;
        }
      }
    },
    "bind" : {
      value : function(name, shortcut, options) {
        if (!name || !shortcut)
          return;
        if (options.load === undefined || options.load) 
          extensions.load(name, options.config);
        bind(shortcut, function () { 
          if (extensions.toggle(name, options.config)) 
            io.notify("Extension " + name + " enabled");
          else 
            io.notify("Extension " + name + " disabled");
        }, options.command);
      }
    }
  });
})();
Object.freeze(extensions);
