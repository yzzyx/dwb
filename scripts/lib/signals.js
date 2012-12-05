(function ()  
{
    var _registered = {};
    function _disconnect(sig) 
    {
        signals[sig] = null;
        delete _registered[sig];
    }
    var _disconnectByProp = function(prop, obj) 
    {
        var sig, i, sigs;
        for (sig in _registered) 
        {
            sigs = _registered[sig];
            for (i = 0; i<sigs.length; i++) 
            {
                if (sigs[i][prop] == obj) 
                {
                    if (_registered[sig].length === 1) 
                    {
                        _disconnect(sig);
                    }
                    else 
                    {
                        sigs[i].connected = false;
                    }
                    return true;
                }
            }
        }
        return false;
    };
    Object.defineProperties(signals, 
    {
        "emit" : 
        {
            value : function(sig, args) 
            {
                var sigs = _registered[sig];
                var ret = false;
                var i = 0, l=sigs.length;
                do 
                {
                    if (sigs[i].connected) 
                    {
                        ret = sigs[i].callback.apply(this, args) || ret;
                        i++;
                    }
                    else 
                    {
                        sigs.splice(i, 1);
                    }
                } while (i<l);

                if (_registered[sig].length === 0) 
                {
                    _disconnect(sig);
                }
                return ret;
            }
        },
        "connect" : 
        {
            value : (function () 
            {
                var id = 0;
                return function(sig, func) 
                {
                    if (func === null || typeof func !== "function") 
                    {
                        return -1;
                    }
                    ++id;
                    if (_registered[sig] === undefined || _registered[sig] === null) 
                    {
                        _registered[sig] = [];
                        signals[sig] = function () { return signals.emit(sig, arguments); };
                    }
                    _registered[sig].push({callback : func, id : id, connected : true });
                    return id;
                };
            })()
        },
        "disconnect" : 
        {
            value : _disconnectByProp.bind(this, "id")
        },
        "disconnectByFunction" : 
        {
            value : _disconnectByProp.bind(this, "callback")
        }, 
        "disconnectByName" : 
        {
            value : function (name) 
            {
                if (signals[name] !== null && signals[name] !== undefined) 
                {
                    _disconnect(name);
                    return true;
                }
                return false;
            }
        }
    });
})();
