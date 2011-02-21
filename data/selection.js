const DwbSelection = {
  followSelection : 
    function () {
      for (var c = window.getSelection().getRangeAt(0).startContainer; c !== document; c = c.parentNode) {
        if (c.href) {
          window.location.href = c.href;
          return;
        }
      }
    },
}
