// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "v8.h"
#include "v8-debug.h"
#include "jsrtutils.h"
#include "chakra_natives.h"

namespace v8 {

  THREAD_LOCAL bool g_EnableDebug = false;
  THREAD_LOCAL bool g_ExposeDebug = false;
  static JsContextRef g_debugContext = JS_INVALID_REFERENCE;

  MessageQueue Debug::messageQueue;
  Debug::MessageHandler Debug::handler = nullptr;

  MessageQueue::MessageQueue() {
    this->waitingForMessage = false;
    this->isProcessingDebuggerMsg = false;
    this->debugEventProcessCount = 0;
    this->msgQueue = new std::queue<std::pair<uint16_t*, int>>();

    int err = uv_sem_init(&newMsgSem, 0);
    CHAKRA_VERIFY(err == 0);
    err = uv_mutex_init(&msgMutex);
    CHAKRA_VERIFY(err == 0);
  }

  MessageQueue::~MessageQueue() {
    uv_mutex_destroy(&msgMutex);
    uv_sem_destroy(&newMsgSem);
    delete this->msgQueue;
    this->msgQueue = nullptr;
  }

  bool MessageQueue::SaveMessage(const uint16_t* command, int length) {
    uint16_t* msg = new uint16_t[length + 1];
    for (int i = 0; i < length; i++) msg[i] = command[i];
    msg[length] = '\0';

    uv_mutex_lock(&msgMutex);
    this->msgQueue->push(std::pair<uint16_t*, int>(msg, length));

    bool isWaiting = this->waitingForMessage;

    if (isWaiting) {
      uv_sem_post(&newMsgSem);
      this->waitingForMessage = false;
    }

    uv_mutex_unlock(&msgMutex);

    return isWaiting;
  }

  std::pair<uint16_t*, int> MessageQueue::PopMessage() {
    uv_mutex_lock(&msgMutex);
    std::pair<uint16_t*, int> msg = this->msgQueue->front();
    this->msgQueue->pop();
    uv_mutex_unlock(&msgMutex);
    return msg;
  }

  bool MessageQueue::IsEmpty() {
    return this->msgQueue->empty();
  }

  void MessageQueue::WaitForMessage() {
    uv_mutex_lock(&msgMutex);

    if (!this->IsEmpty()) {
      uv_mutex_unlock(&msgMutex);
      return;
    }

    this->waitingForMessage = true;
    uv_mutex_unlock(&msgMutex);
    uv_sem_wait(&newMsgSem);
  }

  void MessageQueue::SetProcessMessageFunctions(
    JsValueRef chakraDebugObject,
    JsValueRef processDebuggerMessageFn,
    JsValueRef processDebugEventFn,
    JsValueRef processShouldContinueFn) {
    this->chakraDebugObject = chakraDebugObject;
    this->processDebuggerMessageFn = processDebuggerMessageFn;
    this->processDebugEventFn = processDebugEventFn;
    this->processShouldContinueFn = processShouldContinueFn;
  }

  void MessageQueue::ProcessDebuggerMessage() {
    if (this->isProcessingDebuggerMsg) {
      return;
    }

    if (!this->IsEmpty()) {
      this->isProcessingDebuggerMsg = true;

      std::pair<uint16_t*, int> msg = this->PopMessage();

      v8::MaybeLocal<v8::String> str = v8::String::NewFromTwoByte(
        jsrt::IsolateShim::GetCurrentAsIsolate(),
        msg.first, v8::NewStringType::kNormal, msg.second);

      delete msg.first;

      jsrt::StringUtf8 script;
      JsValueRef ref;
      JsErrorCode errorCode = jsrt::ToString(*str.ToLocalChecked(), &ref, &script);
      CHAKRA_VERIFY_NOERROR(errorCode);

      JsValueRef msgArg;
      errorCode = JsPointerToStringUtf8(script, script.length(), &msgArg);
      CHAKRA_VERIFY_NOERROR(errorCode);

      JsValueRef args[] = { jsrt::GetUndefined(), msgArg };
      JsValueRef responseRef;
      errorCode = JsCallFunction(
        this->processDebuggerMessageFn, args, _countof(args), &responseRef);
      CHAKRA_VERIFY_NOERROR(errorCode);

      JsValueType resultType;
      errorCode = JsGetValueType(responseRef, &resultType);
      CHAKRA_VERIFY_NOERROR(errorCode);

      if (resultType == JsString) {
        v8::Local<v8::String> v8Str =
          MessageImpl::JsValueRefToV8String(responseRef);
        v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
        Debug::handler(*msgImpl);
      }

      this->isProcessingDebuggerMsg = false;
    }
  }

  void MessageQueue::ProcessDebugEvent(
    JsDiagDebugEvent debugEvent,
    JsValueRef eventData) {
    JsValueRef debugEventRef;
    JsErrorCode errorCode = JsIntToNumber(debugEvent, &debugEventRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef args[] = { jsrt::GetUndefined(), debugEventRef, eventData };
    JsValueRef result;
    errorCode = JsCallFunction(
      this->processDebugEventFn, args, _countof(args), &result);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueType resultType;
    errorCode = JsGetValueType(result, &resultType);
    CHAKRA_VERIFY_NOERROR(errorCode);

    if (resultType == JsString) {
      v8::Local<v8::String> v8Str = MessageImpl::JsValueRefToV8String(result);
      v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
      Debug::handler(*msgImpl);
    }
  }

  bool MessageQueue::ShouldContinue(JsDiagDebugEvent debugEvent) {
    JsValueRef debugEventRef;
    JsErrorCode errorCode = JsIntToNumber(debugEvent, &debugEventRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef args[] = { jsrt::GetUndefined(), debugEventRef };
    JsValueRef result;
    errorCode = JsCallFunction(
      this->processShouldContinueFn, args, _countof(args), &result);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueType resultType;
    errorCode = JsGetValueType(result, &resultType);
    CHAKRA_VERIFY_NOERROR(errorCode);

    CHAKRA_VERIFY(resultType == JsBoolean);

    bool shouldContinue = true;
    errorCode = JsBooleanToBool(result, &shouldContinue);
    CHAKRA_VERIFY_NOERROR(errorCode);

    return shouldContinue;
  }

  MessageImpl::MessageImpl(Local<String> v8Str) {
    this->v8Str = v8Str;
  }

  Isolate* MessageImpl::GetIsolate() const {
    return jsrt::IsolateShim::GetCurrentAsIsolate();
  }

  Local<String> MessageImpl::JsValueRefToV8String(JsValueRef result) {
    JsValueRef scriptRef;
    jsrt::StringUtf8 script;

    JsErrorCode errorCode = jsrt::ToString(result, &scriptRef, &script);
    CHAKRA_VERIFY_NOERROR(errorCode);

    v8::MaybeLocal<v8::String> v8Str = v8::String::NewFromUtf8(
      jsrt::IsolateShim::GetCurrentAsIsolate(), script.operator const char *(), v8::NewStringType::kNormal, script.length());

    return v8Str.ToLocalChecked();
  }

  Local<String> MessageImpl::GetJSON() const {
    return this->v8Str;
  }

  bool Debug::EnableAgent(
    const char *name,
    int port,
    bool wait_for_connection) {
#ifdef _WIN32
    HRESULT hr = S_OK;

#ifndef NODE_ENGINE_CHAKRACORE
    if (!g_EnableDebug) {
      // JsStartDebugging needs COM initialization
      IfComFailError(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

      g_EnableDebug = true;

      Local<Context> currentContext = Context::GetCurrent();
      if (!currentContext.IsEmpty()) {
        // Turn on debug mode on current Context (script engine), which was
        // created before start debugging and not in debug mode.
        JsStartDebugging();
      }
    }

  error:
#else
    hr = E_FAIL;  // ChakraCore does not support JsStartDebugging
#endif

    return SUCCEEDED(hr);
#else
    return false;
#endif
  }

  void Debug::ExposeDebug() {
    g_ExposeDebug = true;
  }

  bool Debug::IsDebugExposed() {
    return g_ExposeDebug;
  }

  bool Debug::IsAgentEnabled() {
    return g_EnableDebug;
  }

  Local<Context> Debug::GetDebugContext(Isolate* isolate) {
    jsrt::IsolateShim* isoShim = jsrt::IsolateShim::FromIsolate(isolate);
    JsContextRef debugContextRef = JS_INVALID_REFERENCE;
    if (isoShim->debugContext == JS_INVALID_REFERENCE) {
      HandleScope scope(isolate);
      debugContextRef = *Context::New(isolate);
      isoShim->debugContext = isoShim->GetContextShim(
        debugContextRef);
      JsAddRef(debugContextRef, nullptr);

      Local<Object> global = Context::GetCurrent()->Global();

      // CHAKRA-TODO: Chakra doesn't fully implement the debugger without
      // --debug flag. Add a dummy 'Debug' on global object if it doesn't
      // already exist.
      {
        Context::Scope context_scope(debugContextRef);
        jsrt::ContextShim* contextShim = jsrt::ContextShim::GetCurrent();
        JsValueRef ensureDebug = contextShim->GetensureDebugFunction();
        JsValueRef unused;
        if (jsrt::CallFunction(ensureDebug, *global, &unused) != JsNoError) {
          return Local<Context>();
        }
      }
    }

    return static_cast<Context*>(isoShim->debugContext->GetContextRef());
  }

  void Debug::Dispose() {
#ifdef _WIN32
    if (g_EnableDebug) {
      CoUninitialize();
    }
#endif
  }

  void CHAKRA_CALLBACK JsDiagDebugEventHandler(
    JsDiagDebugEvent debugEvent,
    JsValueRef eventData,
    void* callbackState) {
    if (Debug::handler == nullptr) {
      return;
    }

    Debug::messageQueue.debugEventProcessCount++;
    bool isMsgToProcess = !Debug::messageQueue.IsEmpty();
    bool shouldContinue = true;
    do {
      if (eventData != nullptr) {
        Debug::messageQueue.ProcessDebugEvent(debugEvent, eventData);
        eventData = nullptr;
      }

      // Process all messages received from debugger
      if (Debug::messageQueue.debugEventProcessCount <= 1) {
        isMsgToProcess = !Debug::messageQueue.IsEmpty();
        while (!Debug::messageQueue.isProcessingDebuggerMsg && isMsgToProcess) {
          Debug::messageQueue.ProcessDebuggerMessage();
          isMsgToProcess = !Debug::messageQueue.IsEmpty();
        }
      }

      shouldContinue = Debug::messageQueue.ShouldContinue(debugEvent);
      if (!shouldContinue) {
        Debug::messageQueue.WaitForMessage();
      }

      isMsgToProcess = !Debug::messageQueue.IsEmpty();
    } while (isMsgToProcess &&
      !Debug::messageQueue.isProcessingDebuggerMsg && !shouldContinue);

    Debug::messageQueue.debugEventProcessCount--;
  }

  void Debug::StartDebugging(JsRuntimeHandle runtime) {
    JsErrorCode errorCode = JsDiagStartDebugging(
      runtime, JsDiagDebugEventHandler, nullptr);
    CHAKRA_VERIFY_NOERROR(errorCode);
  }

  void Debug::SendCommand(Isolate* isolate,
    const uint16_t* command, int length,
    ClientData* client_data) {
    CHAKRA_ASSERT(client_data == NULL);

    // Save command in a queue and when JsDiagDebugEventHandler
    // is called we need to process the queue
    if (!messageQueue.SaveMessage(command, length)) {
      // Request async break from engine so that we can process
      // commands on JsDiagDebugEventAsyncBreak
      JsErrorCode errorCode = JsDiagRequestAsyncBreak(
        jsrt::IsolateShim::FromIsolate(isolate)->GetRuntimeHandle());
      CHAKRA_VERIFY_NOERROR(errorCode);
    }
  }

  static JsValueRef CHAKRA_CALLBACK Log(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef scriptRef;
    jsrt::StringUtf8 script;

    if (argumentCount > 1 &&
      jsrt::ToString(arguments[1], &scriptRef, &script) == JsNoError) {
      printf("%s\n", *script);
#ifdef DEBUG
      flushall();
#endif
    }

    return JS_INVALID_REFERENCE;
  }

  static JsValueRef CHAKRA_CALLBACK SendDelayedRespose(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueType resultType;
    JsErrorCode errorCode = JsGetValueType(arguments[1], &resultType);
    CHAKRA_VERIFY_NOERROR(errorCode);

    if (resultType == JsString) {
      v8::Local<v8::String> v8Str = MessageImpl::JsValueRefToV8String(
        arguments[1]);
      v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
      Debug::handler(*msgImpl);
    }

    return JS_INVALID_REFERENCE;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetScripts(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef sourcesList;
    JsErrorCode errorCode = JsDiagGetScripts(&sourcesList);
    CHAKRA_VERIFY_NOERROR(errorCode);

    return sourcesList;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetSource(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    int scriptId;
    JsValueRef source = JS_INVALID_REFERENCE;
    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &scriptId) == JsNoError) {
      JsErrorCode errorCode = JsDiagGetSource(scriptId, &source);
      CHAKRA_VERIFY_NOERROR(errorCode);
    }

    return source;
  }

  static JsValueRef CHAKRA_CALLBACK JsSetStepType(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    bool success = false;
    int stepType;
    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &stepType) == JsNoError) {
      JsErrorCode errorCode = JsDiagSetStepType((JsDiagStepType)stepType);
      CHAKRA_VERIFY_NOERROR(errorCode);
      success = true;
    }

    JsValueRef returnRef = JS_INVALID_REFERENCE;
    JsBoolToBoolean(success, &returnRef);

    return returnRef;
  }

  static JsValueRef CHAKRA_CALLBACK JsSetBreakpoint(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    int scriptId;
    int line;
    int column;
    JsValueRef bpObject = JS_INVALID_REFERENCE;

    if (argumentCount > 3 &&
      jsrt::ValueToInt(arguments[1], &scriptId) == JsNoError &&
      jsrt::ValueToInt(arguments[2], &line) == JsNoError &&
      jsrt::ValueToInt(arguments[3], &column) == JsNoError) {
      JsErrorCode errorCode = JsDiagSetBreakpoint(
        scriptId, line, column, &bpObject);
    }

    return bpObject;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetFunctionPosition(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef valueRef = JS_INVALID_REFERENCE;
    JsErrorCode errorCode = JsConvertValueToObject(arguments[1], &valueRef);
    CHAKRA_VERIFY_NOERROR(errorCode);
    JsValueRef funcInfo = JS_INVALID_REFERENCE;
    errorCode = JsDiagGetFunctionPosition(valueRef, &funcInfo);
    CHAKRA_VERIFY_NOERROR(errorCode);

    return funcInfo;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetStackTrace(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef stackInfo = JS_INVALID_REFERENCE;
    JsErrorCode errorCode = JsDiagGetStackTrace(&stackInfo);
    CHAKRA_VERIFY_NOERROR(errorCode);

    return stackInfo;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetStackProperties(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    int frameIndex;
    JsValueRef properties = JS_INVALID_REFERENCE;
    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &frameIndex) == JsNoError) {
      JsErrorCode errorCode = JsDiagGetStackProperties(frameIndex, &properties);
      CHAKRA_VERIFY_NOERROR(errorCode);
    }

    return properties;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetObjectFromHandle(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef properties = JS_INVALID_REFERENCE;
    int handle;
    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &handle) == JsNoError) {
      JsErrorCode errorCode = JsDiagGetObjectFromHandle(handle, &properties);
    }

    return properties;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetBreakpoints(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef properties = JS_INVALID_REFERENCE;
    JsErrorCode errorCode = JsDiagGetBreakpoints(&properties);
    CHAKRA_VERIFY_NOERROR(errorCode);

    return properties;
  }

  static JsValueRef CHAKRA_CALLBACK JsRemoveBreakpoint(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    int bpId;
    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &bpId) == JsNoError) {
      JsErrorCode errorCode = JsDiagRemoveBreakpoint(bpId);
      CHAKRA_VERIFY_NOERROR(errorCode);
    }

    return JS_INVALID_REFERENCE;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetProperties(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef properties = JS_INVALID_REFERENCE;
    int objectHandle;
    int fromCount;
    int totalCount;

    if (argumentCount > 3 &&
      jsrt::ValueToInt(arguments[1], &objectHandle) == JsNoError &&
      jsrt::ValueToInt(arguments[2], &fromCount) == JsNoError &&
      jsrt::ValueToInt(arguments[3], &totalCount) == JsNoError) {
      JsErrorCode errorCode = JsDiagGetProperties(
        objectHandle, fromCount, totalCount, &properties);
      CHAKRA_VERIFY_NOERROR(errorCode);
    }
    return properties;
  }

  static JsValueRef CHAKRA_CALLBACK JsEvaluate(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    int frameIndex;
    JsValueRef scriptRef;
    jsrt::StringUtf8 script;
    JsValueRef resultArray = JS_INVALID_REFERENCE;
    JsValueRef result = JS_INVALID_REFERENCE;

    if (argumentCount > 2 &&
      jsrt::ValueToInt(arguments[2], &frameIndex) == JsNoError &&
      jsrt::ToString(arguments[1], &scriptRef, &script) == JsNoError) {
      JsErrorCode errorCode = JsDiagEvaluateUtf8(script, frameIndex, &result);
      if (errorCode != JsNoError && errorCode != JsErrorScriptException) {
        jsrt::Fatal("internal error %s(%d): %d", __FILE__, __LINE__, errorCode);
      }

      JsCreateArray(2, &resultArray);

      bool success = errorCode == JsNoError ? true : false;
      JsValueRef resultBool;
      JsBoolToBoolean(success, &resultBool);

      JsValueRef arrayIndex;
      JsIntToNumber(0, &arrayIndex);
      JsSetIndexedProperty(resultArray, arrayIndex, resultBool);
      JsIntToNumber(1, &arrayIndex);
      JsSetIndexedProperty(resultArray, arrayIndex, result);
    }

    return resultArray;
  }

  static JsValueRef CHAKRA_CALLBACK JsEvaluateScript(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsValueRef scriptRef;
    jsrt::StringUtf8 script;
    JsValueRef result = JS_INVALID_REFERENCE;

    if (argumentCount > 1 &&
      jsrt::ToString(arguments[1], &scriptRef, &script) == JsNoError) {
      JsErrorCode errorCode = JsRunScriptUtf8(
        script, JS_SOURCE_CONTEXT_NONE, "", &result);
      if (errorCode != JsNoError) {
        JsValueRef excep;
        JsGetAndClearException(&excep);
      }
    }

    return result;
  }

  static JsValueRef CHAKRA_CALLBACK JsSetBreakOnException(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    int breakOnExceptionAttributes;
    bool success = false;

    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &breakOnExceptionAttributes) == JsNoError &&
      JsDiagSetBreakOnException(jsrt::IsolateShim::GetCurrent()->GetRuntimeHandle(),
        (JsDiagBreakOnExceptionAttributes)breakOnExceptionAttributes) == JsNoError) {
      success = true;
    }

    JsValueRef returnRef;
    JsBoolToBoolean(success, &returnRef);

    return returnRef;
  }

  static JsValueRef CHAKRA_CALLBACK JsGetBreakOnException(
    JsValueRef callee,
    bool isConstructCall,
    JsValueRef *arguments,
    unsigned short argumentCount,
    void *callbackState) {
    JsDiagBreakOnExceptionAttributes breakOnExceptionAttributes =
      JsDiagBreakOnExceptionAttributeNone;

    JsErrorCode errorCode = JsDiagGetBreakOnException(
      jsrt::IsolateShim::GetCurrent()->GetRuntimeHandle(),
      &breakOnExceptionAttributes);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef returnRef = JS_INVALID_REFERENCE;
    errorCode = JsIntToNumber(breakOnExceptionAttributes, &returnRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    return returnRef;
  }

  void Debug::InstallHostCallback(JsValueRef chakraDebugObject,
    const char *name, JsNativeFunction nativeFunction) {
    JsValueRef nameVar;
    JsErrorCode errorCode = JsPointerToStringUtf8(name, strlen(name), &nameVar);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef funcRef;
    errorCode = JsCreateNamedFunction(
      nameVar, nativeFunction, nullptr, &funcRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsPropertyIdRef propertyIdRef;
    errorCode = JsGetPropertyIdFromNameUtf8(name, &propertyIdRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    errorCode = JsSetProperty(chakraDebugObject, propertyIdRef, funcRef, true);
    CHAKRA_VERIFY_NOERROR(errorCode);
  }

  void Debug::SetChakraDebugObject(JsValueRef chakraDebugObject) {
    JsPropertyIdRef propertyIdRef;

    JsErrorCode errorCode = JsGetPropertyIdFromNameUtf8(
      "ProcessDebuggerMessage", &propertyIdRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef processDebuggerMessageFn;
    errorCode = JsGetProperty(chakraDebugObject,
      propertyIdRef, &processDebuggerMessageFn);
    CHAKRA_VERIFY_NOERROR(errorCode);

    errorCode = JsGetPropertyIdFromNameUtf8("ProcessDebugEvent",
      &propertyIdRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef processDebugEventFn;
    errorCode = JsGetProperty(chakraDebugObject,
      propertyIdRef, &processDebugEventFn);
    CHAKRA_VERIFY_NOERROR(errorCode);

    errorCode = JsGetPropertyIdFromNameUtf8("ProcessShouldContinue",
      &propertyIdRef);
    CHAKRA_VERIFY_NOERROR(errorCode);

    JsValueRef processShouldContinueFn;
    errorCode = JsGetProperty(chakraDebugObject,
      propertyIdRef, &processShouldContinueFn);
    CHAKRA_VERIFY_NOERROR(errorCode);

    Debug::messageQueue.SetProcessMessageFunctions(chakraDebugObject,
      processDebuggerMessageFn, processDebugEventFn, processShouldContinueFn);

    Debug::InstallHostCallback(chakraDebugObject,
      "log", Log);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetScripts", JsGetScripts);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetSource", JsGetSource);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagSetStepType", JsSetStepType);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagSetBreakpoint", JsSetBreakpoint);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetFunctionPosition", JsGetFunctionPosition);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetStackTrace", JsGetStackTrace);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetStackProperties", JsGetStackProperties);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetObjectFromHandle", JsGetObjectFromHandle);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagEvaluateScript", JsEvaluateScript);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagEvaluate", JsEvaluate);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetBreakpoints", JsGetBreakpoints);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetProperties", JsGetProperties);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagRemoveBreakpoint", JsRemoveBreakpoint);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagSetBreakOnException", JsSetBreakOnException);
    Debug::InstallHostCallback(chakraDebugObject,
      "JsDiagGetBreakOnException", JsGetBreakOnException);
    Debug::InstallHostCallback(chakraDebugObject,
      "SendDelayedRespose", SendDelayedRespose);
  }

  void Debug::SetMessageHandler(Isolate* isolate, MessageHandler handler) {
    Debug::handler = handler;
  }

}  // namespace v8
