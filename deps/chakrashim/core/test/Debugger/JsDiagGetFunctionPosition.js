// Global function
var x = 1;
function foo() {
  x = 2;
}
WScript.DumpFunctionInfo(foo);

// Function property
var obj = {
  func : function () {
    WScript.Echo('');
  }
};
WScript.DumpFunctionInfo(obj.func);

var global = WScript.LoadScript("function foo(){}", "samethread", "dummyFileName.js");
WScript.DumpFunctionInfo(global.foo);

var evalFunc = eval('new Function("a", "b", "/*some comments\\r\\n*/    return a + b;")');
WScript.DumpFunctionInfo(evalFunc);

/*some function not at 0 column*/function blah() {
  /* First statement not at 0 */
  var xyz = 1;
}
WScript.DumpFunctionInfo(blah);

// Shouldn't get functionInfo of built-ins
WScript.DumpFunctionInfo(JSON.stringify);
WScript.DumpFunctionInfo(eval);

// Shouldn't get functionInfo of library code
WScript.DumpFunctionInfo(Intl.DateTimeFormat);
WScript.Echo("pass");
