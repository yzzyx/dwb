#!javascript 

var supported = [];
var quvi = "quvi -f best ";
var mplayer = ' --exec "mplayer %u"';
function quviSpawn(wv, o) {
  var host = wv.host;
  for (var i=0; i<supported.length; i++) {
    var s = supported[i];
    if (s.test(host)) {
      system.spawn(quvi + wv.uri + mplayer, null, null);
    }
  }
}

function stdoutCallback(response) {
  var lines = response.split("\n");
  for (var i=0; i<lines.length; i++) {
    try {
      var pattern = lines[i].match(/^\s*\S+/)[0];
      supported.push(new RegExp(pattern.replace(/%/g, "\\")));
    } 
    catch(e) {
      io.print(e);
    }
  }
  signals.connect("loadCommitted", quviSpawn);
}
if (system.spawn("quvi --support", stdoutCallback) & SpawnError.spawnFailed) {
  io.print("\033[31mDWB SCRIPT ERROR:\033[0m Initiating quvi failed, aborting");
}
