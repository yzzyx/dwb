(function () {
    var privProps = [];
    function getPrivateIdx(object, key, identifier) {
      var p;
      for (var i=0, l=privProps.length; i<l; ++i) {
        p = privProps[i];
        if (p.object == object && p.key == key && p.identifier === identifier)
          return i;
      }
      return -1;
    }
    Object.defineProperties(util, { 
        "getBody" :  { 
          value :  function(f) {
            if (f !== null && f !== undefined && f instanceof Function) {
              var m = f.toString().match(/\{([\s\S]*)\}/m)[1];
              return m.replace(/^\s*\/\/.*$/mg,'');
            }
            return null;
          }
        }
    });
    Object.freeze(util);
    if (Object.prototype.setPrivate === undefined && Object.prototype.getPrivate === undefined) {
      Object.defineProperties(Object.prototype, {
        "setPrivate" : { 
          value : function(key, value, identifier) {
            if (!(identifier instanceof Object) && !(identifier instanceof Function)) {
              throw new Error("[setPrivate] identifier is no Object or Function");
            }
            var i = getPrivateIdx(this, key, identifier);
            if (i === -1) {
              if (value !== undefined && value !== null)
                privProps.push({ object : this, key : key, identifier : identifier, value : value });
            }
            else if (value !== null) {
              privProps[i].value = value;
            }
            else {
              privProps.splice(i);
            }
          }
        },
        "getPrivate" : { 
          value : function(key, identifier) {
            var i = getPrivateIdx(this, key, identifier);
            if (i !== -1) 
              return privProps[i].value;
            return null;
          }
        }
      });
    }

    if (Object.prototype.forEach === undefined) {
      Object.defineProperty(Object.prototype, "forEach", { 
          value : function (callback) {
            var key;
            for (key in this) {
              callback(key, this[key], this); 
            }
          }
      });
    }
    if (Array.prototype.fastIndexOf === undefined) {
      Object.defineProperty(Array.prototype, "fastIndexOf", {
          value : function (v) {
            for (var i=0, l=this.length; i<l; ++i) {
              if (this[i] == v)
                return i;
            }
            return -1;
          }
      });
    }
    if (Array.prototype.fastLastIndexOf === undefined) {
      Object.defineProperty(Array.prototype, "fastLastIndexOf", {
          value : function (v) {
            for (var i=this.length-1; i>=0; --i) {
              if (this[i] == v)
                return i;
            }
            return -1;
          }
      });
    }
})();
