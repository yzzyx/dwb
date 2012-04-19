const LoadStatus = { 
  provisional : 0, 
  committed : 1,
  finished : 2,
  firstVisualLayout : 3,
  failed : 4
};
const Modifier = {
  Shift    : 1 << 0,
  Lock	    : 1 << 1,
  Control  : 1 << 2,
  Mod1	    : 1 << 3,
  Mod2	    : 1 << 4,
  Mod3	    : 1 << 5,
  Mod4	    : 1 << 6,
  Mod5	    : 1 << 7,
  Button1  : 1 << 8,
  Button2  : 1 << 9,
  Button3  : 1 << 10,
  Button4  : 1 << 11,
  Button5  : 1 << 12,
  Super    : 1 << 26,
  Hyper    : 1 << 27,
  Meta     : 1 << 28,
  Release  : 1 << 30,
  Modifier : 0x5c001fff
};
const ButtonContext = {
  document   : 1 << 1,
  link       : 1 << 2,
  image      : 1 << 3,
  media      : 1 << 4,
  selection  : 1 << 5,
  editable   : 1 << 6
};
const ClickType = {
  click       : 4,
  doubleClick : 5,
  tripleClick : 6
};
