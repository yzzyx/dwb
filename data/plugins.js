const DwbPlugin = {
  _plugins : [], 
  remove: 
    function (id, values, mimetype) {
      var e = document.getElementById(id);
      var doc = e.ownerDocument;
      for (var i=0; i<DwbPlugin._plugins.length; i++) {
        if (DwbPlugin._plugins[i].id == id) {
          return "__dwb__blocked__";
        }
      }
      var style = doc.defaultView.getComputedStyle(e, null);
      var obj = doc.createElement('div');
      var link = doc.createElement('a');
      link.href = "plugin:" + id;
      link.style.display = 'block';
      link.style.width = style.width;
      link.style.height = style.height;
      obj.style.border = "1px solid";
      obj.style.background="transparent url(data:image/png;base64,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) no-repeat scroll center";
      obj.appendChild(link);
      var element = { id:id, values:values, mimetype:mimetype, tagName:e.tagName, obj:obj };
      DwbPlugin._plugins.push(element);
      e.parentNode.appendChild(obj);
      e.parentNode.removeChild(e);
      return 0;
    },
  click: 
    function(id) {
      for (var i=0; i<DwbPlugin._plugins.length; i++)
        if (DwbPlugin._plugins[i].id == id) {
          var pl = DwbPlugin._plugins[i];
          var doc = pl.obj.ownerDocument;
          var p = doc.createElement('div');
          p.innerHTML = '<' + pl.tagName + ' ' + pl.values + ' />';
          var append = p.firstChild;
          append.zIndex = "2000";
          var par = pl.obj.parentNode;
          par.removeChild(pl.obj);
          par.appendChild(p.firstChild);
          window.scrollBy(0,1);
          window.scrollBy(0,-1);
        }
    },
}
