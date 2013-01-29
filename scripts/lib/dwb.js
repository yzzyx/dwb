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

    var _privProps = [];
    var _getPrivateIdx = function(object, key, identifier) 
    {
        var p;
        for (var i=0, l=_privProps.length; i<l; ++i) 
        {
            p = _privProps[i];
            if (p.object == object && p.key == key && p.identifier === identifier)
                return i;
        }
        return -1;
    };
    var _contexts;

    Object.defineProperties(this, { 
            "provide" : 
            { 
                value : function(name, module) 
                {
                    if (_modules[name]) 
                    {
                        io.debug({ 
                                offset : 1, arguments : arguments,
                                error : new Error("provide: Module " + 
                                                  name + " is already defined!")
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
                        io.debug({ 
                                error : new Error("require : invalid argument (" + 
                                                    JSON.stringify(names) + ")"), 
                                offset : 1, 
                                arguments : arguments 
                        });
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
            "_initAfter" : 
            { 
                value : function() 
                {
                    _initialized = true;
                    for (var i=0, l=_callbacks.length; i<l; i++) 
                        _applyRequired(_callbacks[i].names, _callbacks[i].callback);

                },
                configurable : true
            },
            "_initBefore" : 
            {
                value : function(contexts) 
                {
                    _contexts = contexts;
                    Object.freeze(this);
                },
                configurable : true
            }
    });
    Object.defineProperties(GObject.prototype, {
            "setPrivate" : 
            { 
                value : function(key, value, identifier) 
                {
                    if (!(identifier instanceof Object) && !(identifier instanceof Function)) 
                        throw new Error("[setPrivate] identifier is not an Object or Function");

                    var i = _getPrivateIdx(this, key, identifier);
                    if (i === -1) 
                    {
                        if (value !== undefined && value !== null)
                            _privProps.push({ 
                                    object : this, 
                                    key : key, 
                                    identifier : identifier, 
                                    value : value 
                            });
                    }
                    else if (value !== null) 
                    {
                        _privProps[i].value = value;
                    }
                    else 
                    {
                        _privProps.splice(i);
                    }
                }
            },
            "getPrivate" : 
            { 
                value : function(key, identifier) 
                {
                    var i = _getPrivateIdx(this, key, identifier);
                    if (i !== -1) 
                        return _privProps[i].value;
                    return undefined;
                }
            },
            "notify" : 
            { 
                value : function(name, callback, after) 
                { 
                    return this.connect("notify::" + util.uncamelize(name), callback, after || false);
                }
            },
            "connectBlocked" : 
            { 
                value : function(name, callback, after) 
                { 
                    var sig = this.connect(name, (function() { 
                        this.blockSignal(sig);
                        var result = callback.apply(callback, arguments);
                        this.unblockSignal(sig);
                        return result;
                    }).bind(this));
                    return sig;
                }
            },
            "notifyBlocked" : 
            {
                value : function(name, callback, after) 
                {
                    return this.connectBlocked("notify::" + util.uncamelize(name), callback, after || false);
                }
            }
    });
    Object.defineProperties(Deferred.prototype, {
            "done" : {
                value : function(method) {
                    return this.then(method);
                }
            },
            "fail" : {
                value : function(method) {
                    return this.then(null, method);
                }
            },
            "always" : {
                value : function(method)
                {
                    return this.then(method, method);
                }
            }
    });
})();
Object.preventExtensions(this);
