// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the 'Software'), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

(function () {
  var isAtBreak = false;
  var suspendMessage = null;
  var frames = null;
  var nextScopeHandle = 0;
  var selectedFrame = 0;

  function GetNextScopeHandle() {
    return --nextScopeHandle;
  }

  var nextScriptHandle = 100000;
  function GetNextScriptHandle() {
    return --nextScriptHandle;
  }

  var Logger = (function () {
    var logTypes = {
      API : 1,
      Output : 2,
      Input : 3
    };

    var enabledLogType = [];
    function SetLogTypes(enable) {
      enabledLogType[logTypes.API] = enable;
      enabledLogType[logTypes.Output] = enable;
      enabledLogType[logTypes.Input] = enable;
    }

    SetLogTypes(false);

    var logFunc = function (logType, msg, obj) {
      if (enabledLogType[logType]) {
        var printStr = '';
        if (obj != undefined) {
          printStr = ': ';
          printStr += (typeof obj != 'string') ? JSON.stringify(obj) : obj;
        }

        chakraDebug.log(msg + printStr);
      }
    };

    return {
      LogAPI : function (msg, obj) {
        logFunc(logTypes.API, msg, obj);
      },
      LogOutput : function (msg, obj) {
        logFunc(logTypes.Output, msg, obj);
      },
      LogInput : function (msg, obj) {
        logFunc(logTypes.Input, msg, obj);
      },
      Enabled : function (enable) {
        SetLogTypes(enable);
      }
    };
  })();

  function callHostFunctionNoLog(fn) {
    var args = [].slice.call(arguments, 1);
    var start = new Date();
    var result = fn.apply(undefined, args);
    var end = new Date();
    Logger.LogAPI(fn.name + '(' + args + '):' + (end - start));
    return result;
  }

  function callHostFunction(fn) {
    var args = [].slice.call(arguments, 1);
    var start = new Date();
    var result = fn.apply(undefined, args);
    var end = new Date();
    Logger.LogAPI(fn.name + '(' + args + '):' + (end - start), result);
    return result;
  }

  var DebugManager = (function () {
    var eventSeq = 0;
    var responseSeq = 0;
    var breakScriptId = undefined;

    function GetNextResponseSeq() {
      return responseSeq++;
    }

    var retObj = {};

    retObj.GetNextEventSeq = function () {
      return eventSeq++;
    };

    retObj.MakeResponse = function (debugProtoObj, success) {
      return {
        'seq' : GetNextResponseSeq(),
        'request_seq' : debugProtoObj.seq,
        'type' : 'response',
        'command' : debugProtoObj.command,
        'success' : success,
        'refs' : [],
        'running' : !isAtBreak
      };
    };

    retObj.SetBreakScriptId = function (scriptId) {
      breakScriptId = scriptId;
    };

    retObj.GetBreakScriptId = function () {
      return breakScriptId;
    };

    retObj.ScriptManager = (function () {
      var scriptObjectArray = [];
      var scriptIdToHandleObject = {};
      return {
        GetFileNameFromId: function (scriptId) {
          if (scriptId in scriptObjectArray) {
            return scriptObjectArray[scriptId].fileName;
          } else {
            var scripts = callHostFunction(chakraDebug.JsDiagGetScripts);
            scripts.forEach(function (element, index, array) {
              scriptObjectArray[element.scriptId] = element;
            });

            if (scriptId in scriptObjectArray) {
              return scriptObjectArray[scriptId].fileName;
            }
          }
          return '';
        },
        AddScriptIdToHandleMapping: function (scriptId, handle) {
          scriptIdToHandleObject[scriptId] = handle;
        },
        HaveHandle: function (scriptId) {
          return (scriptId in scriptIdToHandleObject);
        },
        IsScriptHandle: function (handle) {
          for (var id in scriptIdToHandleObject) {
            if (scriptIdToHandleObject[id] == handle) {
              return true;
            }
          }
          return false;
        },
        GetScriptHandle: function (scriptId) {
          return scriptIdToHandleObject[scriptId];
        },
        GetScriptObject: function (handle) {
          for (var id in scriptIdToHandleObject) {
            if (scriptIdToHandleObject[id] == handle && id in scriptObjectArray) {
              var scriptObj = scriptObjectArray[id];
              scriptObj['handle'] = handle;
              return scriptObj;
            }
          }
        }
      };
    })();

    retObj.BreakpointManager = (function () {
      var bpList = [];
      var pendingBpList = [];
      return {
        Add : function (id, type, target) {
          if (id > 0) {
            bpList[id] = {
              type : type,
              target : target
            };
          }
        },
        GetTypeAndTarget : function (id) {
          return bpList[id];
        },
        AddPendingBP : function (reqObj) {
          pendingBpList.push(reqObj);
        },
        SetBreakpoint : function (debugProtoObj, addToPendingList) {
          /* {'command':'setbreakpoint','arguments':{'type':'scriptRegExp','target':'^(.*[\\/\\\\])?test\\.js$','line':2,'condition':0},'type':'request','seq':1} */
          /* {'command':'setbreakpoint','arguments':{'type':'scriptId','target':'19','line':3,'condition':0},'type':'request','seq':1} */
          /* {'command':'setbreakpoint','arguments':{'type':'script','target':'c:\\nodejs\\Test\\Test.js','line':6,'column':0},'type':'request','seq':32} */

          var scriptId = -1;
          var bpType = 'scriptId';

          if (debugProtoObj.arguments) {
            if (debugProtoObj.arguments.type == 'scriptRegExp' || debugProtoObj.arguments.type == 'script') {

              var scriptsArray = callHostFunction(chakraDebug.JsDiagGetScripts);

              var targetRegex = new RegExp(debugProtoObj.arguments.target);

              for (var i = 0; i < scriptsArray.length; ++i) {
                if (debugProtoObj.arguments.type == 'scriptRegExp') {
                  if (scriptsArray[i].fileName && scriptsArray[i].fileName.match(targetRegex)) {
                    scriptId = scriptsArray[i].scriptId;
                    bpType = 'scriptRegExp';
                    break;
                  }
                } else if (debugProtoObj.arguments.type == 'script' && (debugProtoObj.arguments.target == null || debugProtoObj.arguments.target == undefined)) {
                  scriptId = DebugManager.GetBreakScriptId();
                  break;
                } else if (debugProtoObj.arguments.type == 'script' && debugProtoObj.arguments.target) {
                  if (scriptsArray[i].fileName && scriptsArray[i].fileName.toLowerCase() == debugProtoObj.arguments.target.toLowerCase()) {
                    scriptId = scriptsArray[i].scriptId;
                    bpType = 'scriptName';
                    break;
                  }
                }
              }
            } else if (debugProtoObj.arguments.type == 'scriptId') {
              scriptId = parseInt(debugProtoObj.arguments.target);
            }
          }

          var response = DebugManager.MakeResponse(debugProtoObj, false);

          if (typeof scriptId == 'number' && scriptId != -1) {
            var column = 0;
            if (debugProtoObj.arguments && debugProtoObj.arguments.column && (typeof debugProtoObj.arguments.column == 'number')) {
              column = debugProtoObj.arguments.column;
            }
            var bpObject = callHostFunction(chakraDebug.JsDiagSetBreakpoint, scriptId, debugProtoObj.arguments.line, column);
            var bpId = bpObject.breakpointId;

            if (bpId > 0) {
              response['body'] = debugProtoObj.arguments;
              response['body'].actual_locations = [];
              response['body'].breakpoint = bpId;
              response['success'] = true;

              DebugManager.BreakpointManager.Add(bpId, bpType, debugProtoObj.arguments.target);
            }
          } else if (addToPendingList == true) {
            this.AddPendingBP(debugProtoObj);
          }

          return response;
        },
        ProcessPendingBP : function () {
          pendingBpList.forEach(function (debugProtoObj, index, array) {
            DebugManager.BreakpointManager.SetBreakpoint(debugProtoObj, false);
          });
        }
      };
    })();

    retObj.CreateScopeAndRef = function (scopeType, propertiesArray, index, frameIndex) {
      var scopeRef = GetNextScopeHandle();
      var scopeAndRef = {};
      scopeAndRef.scope = {
        'type': scopeType,
        'index': index,
        'frameIndex': frameIndex,
        'object': {
          'ref': scopeRef
        }
      };
      var properties = [];
      for (var propsLen = 0; propsLen < propertiesArray.length; ++propsLen) {
        properties.push({
          'name': propertiesArray[propsLen]['name'],
          'propertyType': 0,
          'ref': propertiesArray[propsLen]['handle']
        });
      }
      scopeAndRef.ref = {
        'handle': scopeRef,
        'type': 'object',
        'className': 'Object',
        'constructorFunction': {
          'ref': 100000
        },
        'protoObject': {
          'ref': 100000
        },
        'prototypeObject': {
          'ref': 100000
        },
        'properties': properties
      };

      return scopeAndRef;
    };

    return retObj;
  })();

  function ConvertEventDataToDebugProtocol(debugEvent, msgJSON) {
    var debugProtocol = undefined;

    // Needs to be in sync with JsDiagDebugEvent
    switch (debugEvent) {
    case 0:
      /* JsDiagDebugEventSourceCompilation */
      var compileMsg = {
        'seq' : DebugManager.GetNextEventSeq(),
        'type' : 'event',
        'event' : 'afterCompile',
        'success' : true,
        'body' : {
          'script' : {
            'type' : 'script',
            'name' : DebugManager.ScriptManager.GetFileNameFromId(msgJSON.scriptId),
            'id' : msgJSON.scriptId,
            'lineOffset' : 0,
            'columnOffset' : 0,
            'lineCount' : msgJSON.lineCount,
            'sourceStart' : '',
            'sourceLength' : msgJSON.sourceLength,
            'scriptType' : 2,
            'compilationType' : 0,
            'text' : DebugManager.ScriptManager.GetFileNameFromId(msgJSON.scriptId) + ' (lines: ' + msgJSON.lineCount + ')'
          }
        },
        'running' : !isAtBreak
      };

      debugProtocol = JSON.stringify(compileMsg);

      DebugManager.BreakpointManager.ProcessPendingBP();

      break;
    case 1:
      /* JsDiagDebugEventCompileError */
      break;
    case 2:
      /* JsDiagDebugEventBreak */
    case 3:
      /* JsDiagDebugEventStepComplete */
    case 4:
      /* JsDiagDebugEventDebuggerStatement */
      var breakMsg = {
        'seq' : DebugManager.GetNextEventSeq(),
        'type' : 'event',
        'event' : 'break',
        'body' : {
          'sourceLine' : msgJSON.line,
          'sourceColumn' : msgJSON.column,
          'sourceLineText' : msgJSON.sourceText,
          'script' : {
            'id' : msgJSON.scriptId,
            'name' : DebugManager.ScriptManager.GetFileNameFromId(msgJSON.scriptId)
          }
        }
      };

      if (msgJSON.breakpointId) {
        breakMsg.body['breakpoints'] = [msgJSON.breakpointId];
      }

      debugProtocol = JSON.stringify(breakMsg);
      DebugManager.SetBreakScriptId(msgJSON.scriptId);
      isAtBreak = true;
      break;

    case 5:
      /* JsDiagDebugEventAsyncBreak */
      if (suspendMessage != null) {
        isAtBreak = true;
        var delayedResponse = JSON.stringify(DebugManager.MakeResponse(suspendMessage, true));
        suspendMessage = null;
        chakraDebug.SendDelayedRespose(delayedResponse);
      }
      break;
    case 6:
      /* JsDiagDebugEventRuntimeException */
      var obj = msgJSON['exception'];

      AddChildrens(obj);

      var breakMsg = {
        'seq' : DebugManager.GetNextEventSeq(),
        'type' : 'event',
        'event' : 'exception',
        'body' : {
          'uncaught' : msgJSON.uncaught,
          'exception' : msgJSON['exception'],
          'sourceLine' : msgJSON.line,
          'sourceColumn' : msgJSON.column,
          'sourceLineText' : msgJSON.sourceText,
          'script' : {
            'id' : msgJSON.scriptId,
            'name' : DebugManager.ScriptManager.GetFileNameFromId(msgJSON.scriptId)
          }
        }
      };
      debugProtocol = JSON.stringify(breakMsg);
      DebugManager.SetBreakScriptId(msgJSON.scriptId);
      isAtBreak = true;
      break;
    default:
      throw new Error('Invalid debugEvent: ' + debugEvent);
      break;
    }

    return debugProtocol;
  }

  function AddChildrens(obj) {
    if ('display' in obj) {
      if (!('value' in obj) || !isFinite(obj['value']) || obj['value'] == null || obj['value'] == undefined) {
        obj['value'] = obj['display'];
      }
      if (!('text' in obj)) {
        obj['text'] = obj['display'];
      }
      if (!('className' in obj)) {
        obj['className'] = obj['type'];
      }
    }

    var PROPERTY_ATTRIBUTE_HAVE_CHILDRENS = 0x1;
    var PROPERTY_ATTRIBUTE_READ_ONLY_VALUE = 0x2;

    if (('propertyAttributes' in obj) && ((obj['propertyAttributes'] & PROPERTY_ATTRIBUTE_HAVE_CHILDRENS) == PROPERTY_ATTRIBUTE_HAVE_CHILDRENS)) {
      var objectHandle = obj['handle'];
      var childProperties = callHostFunction(chakraDebug.JsDiagGetProperties, objectHandle, 0, 1000);

      var propertiesArray = [];

      var properties = childProperties['properties'];
      for (var propsLen = 0; propsLen < properties.length; ++propsLen) {
        propertiesArray.push({
          'name' : properties[propsLen]['name'],
          'propertyType' : 0,
          'ref' : properties[propsLen]['handle']
        });
      }

      properties = childProperties['debuggerOnlyProperties'];
      for (var propsLen = 0; propsLen < properties.length; ++propsLen) {
        propertiesArray.push({
          'name' : properties[propsLen]['name'],
          'propertyType' : 0,
          'ref' : properties[propsLen]['handle']
        });
      }

      obj['properties'] = propertiesArray;
    }
  }

  var DebugProtocolHandler = {
    'scripts' : function (debugProtoObj) {
      /* {'command':'scripts','type':'request','seq':1} */
      /* {'command':'scripts','arguments':{'types':7,'includeSource':true,'ids':[70]},'type':'request','seq':22} */
      /* {'command':'scripts','arguments':{'types':7,'filter':'module.js'},'type':'request','seq':81} */

      var chakraScriptsArray = callHostFunctionNoLog(chakraDebug.JsDiagGetScripts);

      var ids = null;
      var filter = null;
      var includeSource = false;
      if (debugProtoObj.arguments) {
        if (debugProtoObj.arguments.ids) {
          ids = debugProtoObj.arguments.ids;
        }
        if (debugProtoObj.arguments.includeSource) {
          includeSource = true;
        }
        if (debugProtoObj.arguments.filter) {
          filter = debugProtoObj.arguments.filter;
        }
      }

      var body = [];
      chakraScriptsArray.forEach(function (element, index, array) {
        var found = true;

        if (ids != null) {
          found = false;
          for (var i = 0; i < ids.length; ++i) {
            if (ids[i] == element.scriptId) {
              found = true;
              break;
            }
          }
        }

        if (filter != null) {
          found = false;
          if (filter == element.fileName) {
            found = true;
          }
        }

        if (found == true) {
          var scriptObj = {
            /*'handle': element.handle,*/
            'type' : 'script',
            'name' : element.fileName,
            'id' : element.scriptId,
            'lineOffset' : 0,
            'columnOffset' : 0,
            'lineCount' : element.lineCount,
            'sourceStart' : '',
            'sourceLength' : element.sourceLength,
            'scriptType' : 2,
            'compilationType' : 0,
            'text' : element.fileName + ' (lines: ' + element.lineCount + ')'
          };
          if (includeSource == true) {
            var chakraSourceObj = callHostFunctionNoLog(chakraDebug.JsDiagGetSource, element.scriptId);
            scriptObj['source'] = chakraSourceObj.source;
            scriptObj['lineCount'] = chakraSourceObj.lineCount;
            scriptObj['sourceLength'] = chakraSourceObj.sourceLength;
          }
          body.push(scriptObj);
        }
      });

      var response = DebugManager.MakeResponse(debugProtoObj, true);
      response['body'] = body;
      return response;
    },
    'source' : function (debugProtoObj) {
      /* {'command':'source','fromLine':2,'toLine':6,'type':'request','seq':1} */
      var chakraSourceObj = callHostFunctionNoLog(chakraDebug.JsDiagGetSource, DebugManager.GetBreakScriptId());
      var response = DebugManager.MakeResponse(debugProtoObj, true);

      response['body'] = {
        'source' : chakraSourceObj.source,
        'fromLine' : 0,
        'toLine' : chakraSourceObj.lineCount,
        'fromPosition' : 0,
        'toPosition' : chakraSourceObj.sourceLength,
        'totalLines' : chakraSourceObj.lineCount
      };

      return response;
    },
    'continue' : function (debugProtoObj) {
      /* {'command':'continue','type':'request','seq':1} */
      /* {'command':'continue','arguments':{'stepaction':'in','stepcount':1},'type':'request','seq':1} */
      var success = true;
      if (debugProtoObj.arguments && debugProtoObj.arguments.stepaction) {
        var jsDiagSetStepType = 0;
        if (debugProtoObj.arguments.stepaction == 'in') {
          /* JsDiagStepTypeStepIn */
          jsDiagSetStepType = 0;
        } else if (debugProtoObj.arguments.stepaction == 'out') {
          /* JsDiagStepTypeStepOut */
          jsDiagSetStepType = 1;
        } else if (debugProtoObj.arguments.stepaction == 'next') {
          /* JsDiagStepTypeStepOver */
          jsDiagSetStepType = 2;
        } else {
          throw new Error('Unhandled stepaction: ' + debugProtoObj.arguments.stepaction);
        }

        if (!callHostFunction(chakraDebug.JsDiagSetStepType, jsDiagSetStepType)) {
          success = false;
        }
      }
      return DebugManager.MakeResponse(debugProtoObj, success);
    },
    'setbreakpoint' : function (debugProtoObj) {
      return DebugManager.BreakpointManager.SetBreakpoint(debugProtoObj, true);
    },
    'backtrace' : function (debugProtoObj) {
      // {'command':'backtrace','arguments':{'fromFrame':0,'toFrame':1},'type':'request','seq':7}
      var response = DebugManager.MakeResponse(debugProtoObj, true);
      if (frames == null || frames.length == 0) {
        var stackTrace = callHostFunction(chakraDebug.JsDiagGetStackTrace);

        frames = [];
        stackTrace.forEach(function (element, index, array) {
          var thisObj = callHostFunction(chakraDebug.JsDiagEvaluate, 'this', element.index);

          var scriptHandle = -1;
          if(DebugManager.ScriptManager.HaveHandle(element.scriptId)) {
            scriptHandle = DebugManager.ScriptManager.GetScriptHandle(element.scriptId);
          } else {
            scriptHandle = GetNextScriptHandle();
            DebugManager.ScriptManager.AddScriptIdToHandleMapping(element.scriptId, scriptHandle);
          }

          frames.push({
            'type' : 'frame',
            'index' : element.index,
            'handle' : GetNextScopeHandle(),/* element.handle,*/
            'constructCall' : false,
            'atReturn' : false,
            'debuggerFrame' : false,
            'position' : 0,
            'line' : element.line,
            'column' : element.column,
            'sourceLineText' : element.sourceText,
            'func' : {
              'ref' : element.functionHandle
            },
            'script' : {
              'ref': scriptHandle
            },
            'receiver' : {
              'ref' : ('handle' in thisObj) ? thisObj['handle'] : element.functionHandle
            },
            'arguments' : [],
            'locals' : [],
            'scopes' : [],
            'text' : ''
          });
        });
      }

      if (debugProtoObj.arguments && debugProtoObj.arguments.fromFrame != undefined) {
        response['body'] = {
          'fromFrame' : debugProtoObj.arguments.fromFrame,
          'toFrame' : debugProtoObj.arguments.toFrame,
          'totalFrames' : frames.length,
          'frames' : []
        };
        response['refs'] = [];
        for (var i = debugProtoObj.arguments.fromFrame; i < debugProtoObj.arguments.toFrame && i < frames.length; ++i) {
          response['body']['frames'].push(frames[i]);
        }
      } else {
        response['body'] = {
          'fromFrame' : 0,
          'toFrame' : frames.length,
          'totalFrames' : frames.length,
          'frames' : []
        };
        response['body']['frames'] = frames;
        response['refs'] = [];
      }

      return response;
    },
    'lookup' : function (debugProtoObj) {
      // {'command':'lookup','arguments':{'handles':[8,0]},'type':'request','seq':2}
      var handlesResult = {};
      for (var handlesLen = 0; handlesLen < debugProtoObj.arguments.handles.length; ++handlesLen) {
        var handle = debugProtoObj.arguments.handles[handlesLen];
        var handleObject = null;
        if (DebugManager.ScriptManager.IsScriptHandle(handle)) {
          handleObject = DebugManager.ScriptManager.GetScriptObject(handle);
        } else {
          handleObject = callHostFunction(chakraDebug.JsDiagGetObjectFromHandle, handle);
          AddChildrens(handleObject);
        }

        if ('fileName' in handleObject && !('name' in handleObject)) {
          handleObject['name'] = handleObject['fileName'];
        }

        if ('scriptId' in handleObject && !('id' in handleObject)) {
          handleObject['id'] = handleObject['scriptId'];
        }

        handlesResult[handle] = handleObject;
      }

      var response = DebugManager.MakeResponse(debugProtoObj, true);
      response['body'] = {};
      response['body'] = handlesResult;
      response['refs'] = [];

      return response;
    },
    'evaluate' : function (debugProtoObj) {
      var response = DebugManager.MakeResponse(debugProtoObj, true);
      var evalResult = undefined;
      if (isAtBreak) {
        // {'command':'evaluate','arguments':{'expression':'x','disable_break':true,'maxStringLength':10000,'frame':0},'type':'request','seq':35}
        // {'seq':37,'request_seq':35,'type':'response','command':'evaluate','success':true,'body':{'handle':13,'type':'number','value':1,'text':'1'},'refs':[],'running':false}

        var tempSelectedFrame = 0;
        if (debugProtoObj.arguments && (typeof debugProtoObj.arguments.frame == 'number')) {
          tempSelectedFrame = debugProtoObj.arguments.frame;
        } else if (debugProtoObj.arguments && debugProtoObj.arguments.global == true) {
          if (frames != null && frames.length > 0) {
            tempSelectedFrame = frames.length - 1;
          } else {
            var stackTrace = callHostFunction(chakraDebug.JsDiagGetStackTrace);
            tempSelectedFrame = stackTrace[stackTrace.length - 1].index;
          }
        }

        chakraDebug.log('debugProtoObj.arguments.expression ' + debugProtoObj.arguments.expression + ', tempSelectedFrame ' + tempSelectedFrame);
        evalResult = callHostFunction(chakraDebug.JsDiagEvaluate, debugProtoObj.arguments.expression, tempSelectedFrame);

        AddChildrens(evalResult);

        response['body'] = evalResult;
      } else {
        // {'command':'evaluate','arguments':{'expression':'process.pid','global':true},'type':'request','seq':1}
        evalResult = callHostFunction(chakraDebug.JsDiagEvaluateScript, debugProtoObj.arguments.expression);
        response['body'] = {
          'value' : evalResult,
          'text' : new String(evalResult)
        };
      }

      return response;
    },
    'threads' : function (debugProtoObj) {
      var response = DebugManager.MakeResponse(debugProtoObj, true);
      response['body'] = {
        'totalThreads' : 1,
        'threads' : [{
            'current' : true,
            'id' : 1
          }
        ]
      };
      response['refs'] = [];
      return response;
    },
    'setexceptionbreak' : function (debugProtoObj) {
      var enabled = false;
      if (debugProtoObj.arguments && debugProtoObj.arguments.enabled) {
        enabled = debugProtoObj.arguments.enabled;
      }

      var breakOnExceptionAttribute = 0; // JsDiagBreakOnExceptionAttributeNone

      if (enabled && debugProtoObj.arguments && debugProtoObj.arguments.type) {
        if (debugProtoObj.arguments.type == 'all') {
          breakOnExceptionAttribute = 0x1 | 0x2; // JsDiagBreakOnExceptionAttributeUncaught | JsDiagBreakOnExceptionAttributeFirstChance
        } else if (debugProtoObj.arguments.type == 'uncaught') {
          breakOnExceptionAttribute = 0x1; // JsDiagBreakOnExceptionAttributeUncaught
        }
      }

      Logger.LogAPI('breakOnExceptionAttribute', breakOnExceptionAttribute);

      var success = callHostFunction(chakraDebug.JsDiagSetBreakOnException, breakOnExceptionAttribute);

      var response = DebugManager.MakeResponse(debugProtoObj, success);

      response['body'] = {
        'type' : debugProtoObj.arguments.type,
        'enabled' : enabled
      };
      response['refs'] = [];
      return response;
    },
    'scopes' : function (debugProtoObj) {
      var response = DebugManager.MakeResponse(debugProtoObj, false);
      if (frames != null && frames.length > 0) {
        var frameIndex = -1;
        for (var i = 0; i < frames.length; ++i) {
          if (frames[i].index == debugProtoObj.arguments.frameNumber) {
            frameIndex = i;
            break;
          }
        }
        if (frameIndex != -1) {
          var props = callHostFunction(chakraDebug.JsDiagGetStackProperties, frames[frameIndex].index);
          var scopes = [];
          var refs = [];

          if ('thisObject' in props) {
              props['locals'].push(props['thisObject']);
          }

          if ('returnValue' in props) {
              props['locals'].push(props['returnValue']);
          }

          if ('functionCallsReturn' in props) {
              props['functionCallsReturn'].forEach(function (element, index, array) {
                  props['locals'].push(element);
              });
          }

          //var scopesMap = { 'locals': 1, 'globals': 0, 'scopes': 3 };
          if (props['locals'] && props['locals'].length > 0) {
            var scopeAndRef = DebugManager.CreateScopeAndRef(1, props['locals'], frames[frameIndex].index, debugProtoObj.arguments.frame_index);
            scopes.push(scopeAndRef.scope);
            refs.push(scopeAndRef.ref);
          }

          if (props['scopes'] && props['scopes'].length > 0) {
            var allScopeProperties = [];
            for (var scopesLen = 0; scopesLen < props['scopes'].length; ++scopesLen) {
              var scopeHandle = props['scopes'][scopesLen].handle;

              var scopeProperties = callHostFunction(chakraDebug.JsDiagGetProperties, scopeHandle, 0, 1000);

              for (var i = 0; i < scopeProperties['properties'].length; ++i) {
                allScopeProperties.push(scopeProperties['properties'][i]);
              }
              for (var i = 0; i < scopeProperties['debuggerOnlyProperties'].length; ++i) {
                allScopeProperties.push(scopeProperties['debuggerOnlyProperties'][i]);
              }
            }

            if (allScopeProperties.length > 0) {
              var scopeAndRef = DebugManager.CreateScopeAndRef(3, allScopeProperties, frames[frameIndex].index, debugProtoObj.arguments.frame_index);
              scopes.push(scopeAndRef.scope);
              refs.push(scopeAndRef.ref);
            }
          }

          if (props['globals'] && props['globals'].handle) {
            var globalsProps = callHostFunction(chakraDebug.JsDiagGetProperties, props['globals'].handle, 0, 5000);

            var globalProperties = [];
            for (var i = 0; i < globalsProps['properties'].length; ++i) {
              globalProperties.push(globalsProps['properties'][i]);
            }
            for (var i = 0; i < globalsProps['debuggerOnlyProperties'].length; ++i) {
              globalProperties.push(globalsProps['debuggerOnlyProperties'][i]);
            }

            if (globalProperties.length > 0) {
              var scopeAndRef = DebugManager.CreateScopeAndRef(0, globalProperties, frames[frameIndex].index, debugProtoObj.arguments.frame_index);
              scopes.push(scopeAndRef.scope);
              refs.push(scopeAndRef.ref);
            }
          }

          response['success'] = true;
          response['refs'] = refs;
          response['body'] = {
            'fromScope' : 0,
            'toScope' : 1,
            'totalScopes' : 1,
            'scopes' : scopes
          };
        }
      }
      return response;
    },
    'scope': function (debugProtoObj) {
      // {"seq":9,"type":"request","command":"scope","arguments":{"number":1"frameNumber":1}}
      var response = DebugManager.MakeResponse(debugProtoObj, false);
      if (frames != null && frames.length > 0) {
        var tempSelectedFrame = (debugProtoObj.arguments && debugProtoObj.arguments.frameNumber) ? debugProtoObj.arguments.frameNumber : 0;
        var frameIndex = -1;

        for (var i = 0; i < frames.length; ++i) {
          if (frames[i].index == tempSelectedFrame) {
            frameIndex = i;
            break;
          }
        }

        if (debugProtoObj.arguments && debugProtoObj.arguments.functionHandle) {
          var childProperties = callHostFunction(chakraDebug.JsDiagGetProperties, debugProtoObj.arguments.functionHandle, 0, 1000);
        }

        Logger.LogAPI('frameIndex', frameIndex);

        if (frameIndex != -1) {
          var props = callHostFunction(chakraDebug.JsDiagGetStackProperties, frames[frameIndex].index);
          var scopes = [];
          var refs = [];

          if ('thisObject' in props) {
              props['locals'].push(props['thisObject']);
          }

          if ('returnValue' in props) {
            props['locals'].push(props['returnValue']);
          }

          if ('functionCallsReturn' in props) {
            props['functionCallsReturn'].forEach(function (element, index, array) {
              props['locals'].push(element);
            });
          }

          //var scopesMap = { 'locals': 1, 'globals': 0, 'scopes': 3 };
          if (props['locals'] && props['locals'].length > 0) {
            var scopeAndRef = DebugManager.CreateScopeAndRef(1, props['locals'], frames[frameIndex].index, debugProtoObj.arguments.frame_index);
            scopes.push(scopeAndRef.scope);
            refs.push(scopeAndRef.ref);
          }

          if (props['scopes'] && props['scopes'].length > 0) {
            var allScopeProperties = [];
            for (var scopesLen = 0; scopesLen < props['scopes'].length; ++scopesLen) {
              var scopeHandle = props['scopes'][scopesLen].handle;

              var scopeProperties = callHostFunction(chakraDebug.JsDiagGetProperties, scopeHandle, 0, 1000);

              for (var i = 0; i < scopeProperties['properties'].length; ++i) {
                allScopeProperties.push(scopeProperties['properties'][i]);
              }
              for (var i = 0; i < scopeProperties['debuggerOnlyProperties'].length; ++i) {
                allScopeProperties.push(scopeProperties['debuggerOnlyProperties'][i]);
              }
            }

            if (allScopeProperties.length > 0) {
              var scopeAndRef = DebugManager.CreateScopeAndRef(3, allScopeProperties, frames[frameIndex].index, debugProtoObj.arguments.frame_index);
              scopes.push(scopeAndRef.scope);
              refs.push(scopeAndRef.ref);
            }
          }

          if (props['globals'] && props['globals'].handle) {
            var globalsProps = callHostFunction(chakraDebug.JsDiagGetProperties, props['globals'].handle, 0, 5000);

            var globalProperties = [];
            for (var i = 0; i < globalsProps['properties'].length; ++i) {
              globalProperties.push(globalsProps['properties'][i]);
            }
            for (var i = 0; i < globalsProps['debuggerOnlyProperties'].length; ++i) {
              globalProperties.push(globalsProps['debuggerOnlyProperties'][i]);
            }

            if (globalProperties.length > 0) {
              var scopeAndRef = DebugManager.CreateScopeAndRef(0, globalProperties, frames[frameIndex].index, debugProtoObj.arguments.frame_index);
              scopes.push(scopeAndRef.scope);
              refs.push(scopeAndRef.ref);
            }
          }

          var scopeIndex = (debugProtoObj.arguments && debugProtoObj.arguments.number) ? debugProtoObj.arguments.number : 0;

          Logger.LogAPI('scopeIndex', scopeIndex);
          Logger.LogAPI('scopes', scopes);

          if (scopes.length > scopeIndex) {
            response['success'] = true;
            response['refs'] = refs;
            response['body'] = scopes[scopeIndex];
            response['refs'] = [refs[scopeIndex]];
          }
        }
      }
      return response;
    },
    'listbreakpoints' : function (debugProtoObj) {
      // {'command':'listbreakpoints','type':'request','seq':25}
      var breakpoints = callHostFunction(chakraDebug.JsDiagGetBreakpoints);

      var breakOnExceptionAttribute = callHostFunction(chakraDebug.JsDiagGetBreakOnException);

      var response = DebugManager.MakeResponse(debugProtoObj, true);
      response['body'] = {
        'breakpoints' : [],
        'breakOnExceptions': breakOnExceptionAttribute != 0,
        'breakOnUncaughtExceptions': (breakOnExceptionAttribute & 0x1) == 0x1
      };

      for (var i = 0; i < breakpoints.length; ++i) {
        var bpObj = {
          'number' : breakpoints[i].breakpointId,
          'line' : breakpoints[i].line,
          'column' : breakpoints[i].column,
          'groupId' : null,
          'hit_count' : 0,
          'active' : true,
          'condition' : null,
          'ignoreCount' : 0,
          'actual_locations' : [{
              'line' : breakpoints[i].line,
              'column' : breakpoints[i].column,
              'script_id' : breakpoints[i].scriptId
            }
          ],
          'type' : 'scriptId',
          'script_id' : breakpoints[i].scriptId,
          'script_name' : DebugManager.ScriptManager.GetFileNameFromId(breakpoints[i].scriptId)
        };

        var bpTypeObj = DebugManager.BreakpointManager.GetTypeAndTarget(breakpoints[i].breakpointId);

        if (bpTypeObj) {
          bpObj['type'] = bpTypeObj.type;
          if (bpTypeObj.type == 'scriptRegExp') {
            bpObj['script_regexp'] = bpTypeObj.target;
            // Need to fill in something?
          }
        }

        response['body']['breakpoints'].push(bpObj);
      }
      return response;
    },
    'clearbreakpoint' : function (debugProtoObj) {
      // {'command':'clearbreakpoint','arguments':{'breakpoint':2},'type':'request','seq':39}
      var response = DebugManager.MakeResponse(debugProtoObj, true);
      callHostFunction(chakraDebug.JsDiagRemoveBreakpoint, debugProtoObj.arguments.breakpoint);
      return response;
    },
    'suspend' : function (debugProtoObj) {
      // {'command':'suspend','type':'request','seq':26}
      suspendMessage = debugProtoObj;
    }
  };

  function DebugProtocolToChakra(debugProtoJSON) {
    var debugProtoObj = JSON.parse(debugProtoJSON);
    var returnJSON = undefined;

    switch (debugProtoObj.command) {
    case 'scripts':
    case 'source':
    case 'setbreakpoint':
    case 'backtrace':
    case 'lookup':
    case 'listbreakpoints':
    case 'clearbreakpoint':
    case 'suspend':
    case 'evaluate':
    case 'scopes':
    case 'scope':
    case 'threads':
    case 'setexceptionbreak':
      returnJSON = DebugProtocolHandler[debugProtoObj.command](debugProtoObj);
      break;
    case 'continue':
      isAtBreak = false;
      frames = null;
      returnJSON = DebugProtocolHandler[debugProtoObj.command](debugProtoObj);
      break;
      // Not handled, return failure response
    case 'break':
    case 'changebreakpoint':
    case 'changelive':
    case 'clearbreakpointgroup':
    case 'disconnect':
    case 'flags':
    case 'frame':
    case 'gc':
    case 'references':
    case 'restartframe':
    case 'setvariablevalue':
    case 'v8flag':
    case 'version':
      returnJSON = DebugManager.MakeResponse(debugProtoObj, false);
      break;
    default:
      throw new Error('Unhandled command: ' + debugProtoObj.command);
      break;
    }

    return returnJSON;
  }

  function GetSourceContextOfFunction(compiledWrapper, line, column) {
    Logger.LogAPI('GetSourceContextOfFunction(' + compiledWrapper + ',' + line + ',' + column + ')');
    var funcInfo = callHostFunction(chakraDebug.JsDiagGetFunctionPosition, compiledWrapper);

    if (funcInfo.scriptId >= 0) {
      var bpLine = funcInfo.firstStatementLine + line;
      var bpColumn = funcInfo.firstStatementColumn + column;
      var bpObject = callHostFunction(chakraDebug.JsDiagSetBreakpoint, funcInfo.scriptId, bpLine, bpColumn);
      var bpId = bpObject.breakpointId;

      DebugManager.BreakpointManager.Add(bpId, 'scriptId', funcInfo.id);

      return bpId;
    }
    return -1;
  }

  function DebugEventToString(debugEvent) {
    switch (debugEvent) {
    case 0:
      return 'JsDiagDebugEventSourceCompile';
    case 1:
      return 'JsDiagDebugEventCompileError';
    case 2:
      return 'JsDiagDebugEventBreak';
    case 3:
      return 'JsDiagDebugEventStepComplete';
    case 4:
      return 'JsDiagDebugEventDebuggerStatement';
    case 5:
      return 'JsDiagDebugEventAsyncBreak';
    case 6:
      return 'JsDiagDebugEventRuntimeException';
    default:
      return 'Unhandled JsDiagDebugEvent: ' + debugEvent;
    }
  }

  chakraDebug = {
    'ProcessDebugProtocolJSON' : function (debugProtoJSON) {
      Logger.LogInput('ProcessDebugProtocolJSON debugProtoJSON', debugProtoJSON);
      try {
        var debugProtoJSONReturn = JSON.stringify(DebugProtocolToChakra(debugProtoJSON));
        Logger.LogInput('ProcessDebugProtocolJSON debugProtoJSONReturn', debugProtoJSONReturn);
        return debugProtoJSONReturn;
      } catch (ex) {
        Logger.LogInput('ProcessDebugProtocolJSON exception: ' + ex.message, ex.stack);
      }
    },
    'ProcessJsrtEventData' : function (debugEvent, eventData) {
      Logger.LogInput('ProcessJsrtEventData debugEvent: ' + DebugEventToString(debugEvent) + ', eventData: ' + JSON.stringify(eventData));
      try {
        var debugProtoJSONReturn = ConvertEventDataToDebugProtocol(debugEvent, eventData);
        Logger.LogOutput('ProcessJsrtEventData debugEvent: ' + DebugEventToString(debugEvent) + ', debugProtoJSONReturn: ' + debugProtoJSONReturn);
        return debugProtoJSONReturn;
      } catch (ex) {
        Logger.LogInput('ProcessJsrtEventData exception: ' + ex.message, ex.stack);
      }
    },
    'DebugObject' : {
      'Debug' : {
        'setListener' : function (fn) {},
        'setBreakPoint' : function (compiledWrapper, line, column) {
          GetSourceContextOfFunction(compiledWrapper, line, column);
        },
        'Logging' : function (enable) {
          Logger.Enabled(enable);
        }
      }
    }
  };

  Object.defineProperty(chakraDebug, 'shouldContinue', {
    get : function () {
      //Logger.LogAPI('shouldContinue: ', !isAtBreak);
      return !isAtBreak;
    }
  });

  this.chakraDebug = chakraDebug;

  return this.chakraDebug;
});
