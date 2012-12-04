(function() {
    var _modules = {};
    var _requires = {};
    var _callbacks = [];
    var _initialized = false;
    var _applyRequired = function(names, callback) 
    {
        if (names === null) 
            callback.call(this, _modules);
        else 
        {
            var modules = [];
            var name, detail;
            for (var j=0, m=names.length; j<m; j++) 
            {
                name = names[j];
                if (/^\w*!/.test(name)) 
                {
                    detail = name.split("!");
                    name = detail[0];
                    if (!_modules[name]) 
                        include(detail.slice(1).join("!"));
                }
                modules.push(_modules[name]);
            } 
            if (callback)
                callback.apply(this, modules);
        }
    };
    Object.defineProperties(this, { 
            "provide" : 
            { 
                value : function(name, module) 
                {
                    if (_modules[name]) 
                    {
                        io.debug({ 
                                offset : 1, arguments : arguments,
                                error : new Error("provide: Module " + name + " is already defined!")
                        });
                    }
                    else 
                        _modules[name] = module;
                }
            },
            "replace" : 
            {
                value : function(name, module) 
                {
                    if (! module && _modules[name] ) 
                    {
                        for (var key in _modules[name]) 
                            delete _modules[name][key];

                        delete _modules[name];
                    }
                    else 
                        _modules[name] = module;
                }
            },
            "require" : 
            {
                value : function(names, callback) 
                {
                    if (names !== null && ! (names instanceof Array)) 
                    {
                        io.debug({ error : new Error("require : invalid argument (" + JSON.stringify(names) + ")"), offset : 1, arguments : arguments });
                        return; 
                    }

                    if (!_initialized) 
                        _callbacks.push({callback : callback, names : names});
                    else 
                        _applyRequired(names, callback);
                }
            },
            // Called after all scripts have been loaded and executed
            // Immediately deleted from the global object, so it is not callable
            // from other scripts
            "_init" : 
            { 
                value : function() 
                {
                    _initialized = true;
                    for (var i=0, l=_callbacks.length; i<l; i++) 
                        _applyRequired(_callbacks[i].names, _callbacks[i].callback);

                    Object.freeze(this);
                },
                configurable : true
            }
    });
    Object.defineProperty(GObject.prototype, "notify", { 
            value : function(name, callback, after) 
            { 
                return this.connect("notify::" + util.uncamelize(name), callback, after || false);
            }
    });
})();
Object.preventExtensions(this);
