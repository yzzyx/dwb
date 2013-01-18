(function () {
    Object.defineProperties(util, 
    { 
        "getBody" :  
        { 
            value :  function(f) 
            {
                if (f !== null && f !== undefined && f instanceof Function) 
                {
                    var m = f.toString().match(/\{([\s\S]*)\}/m)[1];
                    m = m.replace(/^\s*\/\/.*$/mg, '');
                    m = m.replace(/^[ \t\r\v\f]*/, '');
                    if (m[0] == "\n")
                        return m.substring(1);
                    else 
                        return m;
                }
                return null;
            }
        },
        "getSelection" : 
        {
            value : function() 
            {
                var frames = tabs.current.allFrames;
                for (var i=frames.length-1; i>=0; --i) 
                {
                    var selection = JSON.parse(frames[i].inject("return document.getSelection().toString()"));
                    if (selection.length > 0)
                        return selection;
                }
                return null;
            }
        },
        "uncamelize" : 
        {
            value : function(text) 
            {
                if (! text || text.length === 0)
                    return text;

                var c = text.charAt(0);
                var uncamelized = c == c.toUpperCase() ? c.toLowerCase() : c;
                for (var i=1, l=text.length; i<l; ++i)
                {
                    c = text.charAt(i);
                    uncamelized += c.toUpperCase() == c ? "-" + c.toLowerCase() : c;
                }
                return uncamelized;
            }
        },
        "camelize" : 
        {
            value : function(text) 
            {
                if (! text || text.length === 0)
                    return text;

                var camelized = "", c;
                for (var i=0, l=text.length; i<l; ++i)
                {
                    c = text.charAt(i);
                    if (c == "-" || c == "_") {
                        i++;
                        camelized += text.charAt(i).toUpperCase();
                    }
                    else 
                        camelized += c;
                }
                return camelized;
            }
        }
    });
    Object.freeze(util);
    

    if (Object.prototype.forEach === undefined) 
    {
        Object.defineProperty(Object.prototype, "forEach", 
        { 
            value : function (callback) 
            {
                var key;
                for (key in this) 
                    callback(key, this[key], this); 
            }
        });
    }
    if (Array.prototype.fastIndexOf === undefined) 
    {
        Object.defineProperty(Array.prototype, "fastIndexOf", 
        {
            value : function (v) 
            {
                for (var i=0, l=this.length; i<l; ++i) {
                    if (this[i] == v)
                        return i;
                }
                return -1;
            }
        });
    }
    if (Array.prototype.fastLastIndexOf === undefined) 
    {
        Object.defineProperty(Array.prototype, "fastLastIndexOf", 
        {
            value : function (v) 
            {
                for (var i=this.length-1; i>=0; --i) {
                    if (this[i] == v)
                        return i;
                }
                return -1;
            }
        });
    }
})();
