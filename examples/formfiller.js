#!javascript

/* 
 * Formfiller that automatically fills html forms. To get form names the
 * webinspector can be used (needs 'enable-developer-extras' to be enabled.)
 *
 * The script has 2 configurable variables: 
 *
 * shorcut:   the shortcut that fills the form, uses dwb's default shortcut
 *            syntax
 *
 * forms:     form data that will be filled into the form, the form data entry
 *            follows the following format: 
 *
 * "[url regexp]" : { submit : [whether to autosubmit form], form { [name attribute of input] : [value], .... }, 
 *
 *
 * */ 

var shortcut = "gF";

var forms = { 
  "http://www.example.com/.*" : {  submit : true,   form : {  name : "name", pass : "password", savepass : "true" } },
  "http://www.foo.com.uk/.*" :  {  submit : false,  form : {  foo_name : "foo", foo_pass : "bar" } }
};

var injectFunction = function (data, submit) {
  var name, value;
  var e;
  for (name in data) {
    value = data[name];
    e = document.getElementsByName(name)[0];
    if(e.type=="checkbox" || e.type=="radio") 
      e.checked=(value.toLowerCase() !== 'false' && value !== '0');
    else
      e.value=value;
  }
  if (submit)
    e.form.submit();
};

function fillForm () {
  var key, r, name;
  var uri = tabs.current.uri;
  for (key in forms) {
    r = new RegExp(key);
    if (r.test(uri)) {
      var script = "(" + String(injectFunction) + ")(" + JSON.stringify(forms[key].form) + "," +  forms[key].submit + ");";
      var frames = tabs.current.allFrames;
      for (var i=0; i<frames.length; i++) {
        frames[i].inject(script);
      }
      return;
    }
  }
}

bind(shortcut, fillForm);

// vim: set ft=javascript:
