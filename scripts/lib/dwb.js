(function() {
    var _modules = {};
    var _requires = {};
    var _callbacks = [];
    var _initialized = false;
    var _applyRequired = function(names, callback) {
      var modules = [];
      for (var j=0, m=names.length; j<m; j++) {
        modules.push(_modules[names[j]]);
      }
      callback.apply(this, modules);
    };
    Object.defineProperties(this, { 
        provide : { 
          value : function(name, module) {
            if (_modules[name]) {
              io.debug("provide: Module " + name + " is already defined!");
            }
            else 
              _modules[name] = module;
          }
        },
        require : {
          value : function(names, callback) {
            if (!_initialized) {
              _callbacks.push({callback : callback, names : names});
            }
            else {
              _applyRequired(names, callback);
            }
          }
        },
        // Immediately deleted from the global object
        _init : { 
          value : function() {
            _initialized = true;
            var names, callback, modules, name;
            for (var i=0, l=_callbacks.length; i<l; i++) {
              _applyRequired(_callbacks[i].names, _callbacks[i].callback);
            }
            Object.freeze(this);
        },
        configurable : true
      }
  });
})();
Object.preventExtensions(this);
