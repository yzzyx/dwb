(function () {
  var configDir = data.configDir;
  var profile = data.profile;
  Object.defineProperties(data, {
      "bookmarks" :  { value : configDir + "/" + profile + "/bookmarks", enumerable : true }, 
      "history"   :  { value : configDir + "/" + profile + "/history", enumerable : true },
      "cookies"   :  { value : configDir + "/" + profile + "/cookies", enumerable : true }, 
      "quickmarks" : { value : configDir + "/" + profile + "/cookies", enumerable : true },
      "cookies"   :  { value : configDir + "/" + profile + "/cookies", enumerable : true }, 
      "cookiesWhitelist"  :  { value : configDir + "/" + profile + "/cookies.allow", enumerable : true }, 
      "sessionCookiesWhitelist"   :  { value : configDir + "/" + profile + "/cookies_session.allow", enumerable : true }, 
      "sessionCookiesWhitelist"   :  { value : configDir + "/" + profile + "/cookies_session.allow", enumerable : true }, 
      "pluginsWhitelist"  :  { value : configDir + "/" + profile + "/plugins.allow", enumerable : true }, 
      "scriptWhitelist"   :  { value : configDir + "/" + profile + "/scripts.allow", enumerable : true }, 
      "session"   :  { value : configDir + "/" + profile + "/session", enumerable : true }, 
      "customKeys"  :  { value : configDir + "/" + profile + "/custom_keys", enumerable : true }, 
      "keys"  :  { value : configDir + "/keys", enumerable : true }, 
      "settings"  :  { value : configDir + "/settings", enumerable : true }, 
      "searchEngines"   :  { value : configDir + "/searchengines", enumerable : true }, 
      "cookies"   :  { value : configDir + "/" + profile + "/cookies", enumerable : true }, 
      "quickmarks"  :  { value : configDir + "/" + profile + "/cookies", enumerable : true }, 
      "cookies"   :  { value : configDir + "/" + profile + "/cookies", enumerable : true }, 
      "cookiesWhitelist"  :  { value : configDir + "/" + profile + "/cookies.allow", enumerable : true }, 
      "sessionCookiesWhitelist"   :  { value : configDir + "/" + profile + "/cookies_session.allow", enumerable : true }, 
      "sessionCookiesWhitelist"   :  { value : configDir + "/" + profile + "/cookies_session.allow", enumerable : true }, 
      "pluginsWhitelist"  :  { value : configDir + "/" + profile + "/plugins.allow", enumerable : true }, 
      "scriptWhitelist"   :  { value : configDir + "/" + profile + "/scripts.allow", enumerable : true }, 
      "session"   :  { value : configDir + "/" + profile + "/session", enumerable : true }, 
      "customKeys"  :  { value : configDir + "/" + profile + "/custom_keys", enumerable : true }, 
      "keys"  :  { value : configDir + "/keys", enumerable : true }, 
      "settings"  :  { value : configDir + "/settings", enumerable : true }, 
      "searchEngines"   :  { value : configDir + "/searchengines", enumerable : true }
  });
})();
