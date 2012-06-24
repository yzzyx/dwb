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
})();
