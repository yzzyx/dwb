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
      function getPlugin(name, filename) {
        var ret = null;
        try {
          if (io.fileTest(filename, FileTest.exists)) {
            ret = include(filename);
          }
        }
        catch(e) {
          extensions.error(name, "Error in line " + e.line + " parsing " + filename);
        }
        return ret;
      }

      return function(name) {
        var boldname = "\033[1m" + name + "\033[0m";

        var config, dataBase, pluginPath, plugin = null;
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
      };
    })()
  }
});
