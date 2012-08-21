(function () {
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
})();
