// Automatically go in insertmode, if the active element 
// is a textinput.
function insert_mode() {
  setTimeout(function() { 
    if (document.activeElement instanceof HTMLInputElement) 
      console.log("_dwb_input_mode_");
  }, 100);
}
window.addEventListener('load',  insert_mode, false);

