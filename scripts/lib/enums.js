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
const NavigationReason = {
  linkClicked     : 0,
  formSubmitted   : 1,
  backForward     : 2,
  reload          : 3,
  formResubmitted : 4,
  other           : 5
};
const DownloadStatus = {
  error       : -1,
  created   : 0,
  started   : 1, 
  cancelled : 2,
  finished  : 3
};
const SpawnError = {
  success       : 0, 
  spawnFailed   : 1<<0, 
  stdoutFailed  : 1<<1, 
  stderrFailed  : 1<<2
};
const ChecksumType = {
  md5     : 0, 
  sha1    : 1, 
  sha256  : 2 
};
const FileTest = {
  regular    : 1 << 0,
  symlink    : 1 << 1,
  dir        : 1 << 2,
  executable : 1 << 3,
  exists     : 1 << 4
};
const Modes = {
  NormalMode  : 1<<0,
  InsertMode  : 1<<1,
  CommandMode : 1<<2
};
