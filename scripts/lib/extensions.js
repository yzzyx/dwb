Object.defineProperties(extensions, { 
  "warning" : {
    value : function (name, message) {
      io.print("\033[1mDWB EXTENSION WARNING: \033[0mextension  \033[1m" + name + "\033[0m: " + message, "stderr");
    }
  }, 
  "error" : {
    value : function (name, message) {
      io.print("\033[31mDWB EXTENSION ERROR: \033[0mextension \033[1m" + name + "\033[0m: " + message, "stderr");
    }
  },
  "message" : {
    value : function (name, message) {
      io.print("\033[1mDWB EXTENSION: \033[0mextension \033[1m" + name + "\033[0m: " + message, "stderr");
    }
  },
  "load" : {
    value : (function () {
      var _config = undefined;
      return function(name) {
        var boldname = "\033[1m" + name + "\033[0m";

        var config, dataBase, pluginPath, plugin;
        var extConfig = null;

        /* Get default config if the config hasn't been read yet */
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

        /* Load extension */
        plugin = include(data.userDataDir + "/extensions/" + name);
        if (plugin === null) {
          plugin = include(data.systemDataDir + "/extensions/" + name);
          if (plugin === null) {
            extensions.error(name, "Couldn't find extension.");
            return false;
          }

        }
        if (plugin.init(extConfig)) {
          extensions.message(name, "Successfully loaded and initialized.");
          return true;
        }
        else {
          extensions.error(name, "Initialization failed.");
          return false;
        }
      };
    })()
  }
});
