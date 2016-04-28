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

  __declspec(thread) bool g_EnableDebug = false;
  __declspec(thread) bool g_ExposeDebug = false;
  static JsContextRef g_debugContext = JS_INVALID_REFERENCE;

  MessageQueue Debug::messageQueue;
  Debug::MessageHandler Debug::handler = nullptr;

  MessageQueue::MessageQueue() {
    this->waitingForMessage = false;
    this->isProcessingDebuggerMsg = false;
    this->msgQueue = new std::queue<wchar_t*>();

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

  void MessageQueue::SaveMessage(const uint16_t* command, int length) {
    uint16_t* msg = new uint16_t[length + 1];
    for (int i = 0; i < length; i++) msg[i] = command[i];
    msg[length] = '\0';

    uv_mutex_lock(&msgMutex);
    this->msgQueue->push((wchar_t*)msg);

    if (this->waitingForMessage) {
      uv_sem_post(&newMsgSem);
      this->waitingForMessage = false;
    }

    uv_mutex_unlock(&msgMutex);
  }

  wchar_t* MessageQueue::PopMessage() {
    uv_mutex_lock(&msgMutex);
    wchar_t* msg = this->msgQueue->front();
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

  void MessageQueue::SetProcessMessage(
      JsValueRef chakraDebugObject,
      JsValueRef processDebugProtocolJSON,
      JsValueRef processJsrtEventData) {
    this->chakraDebugObject = chakraDebugObject;
    this->processDebugProtocolJSON = processDebugProtocolJSON;
    this->processJsrtEventData = processJsrtEventData;
  }

  bool MessageQueue::ProcessMessage(wchar_t* msg) {
    JsValueRef msgArg;
    JsErrorCode errorCode = JsPointerToString(msg, wcslen(msg), &msgArg);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef args[] = { jsrt::GetUndefined(), msgArg };
    JsValueRef result;
    errorCode = JsCallFunction(
        this->processDebugProtocolJSON, args, _countof(args), &result);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsPropertyIdRef propertyIdRef;
    errorCode = JsGetPropertyIdFromName(L"shouldContinue", &propertyIdRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef shouldContinueRef;
    errorCode = JsGetProperty(
        this->chakraDebugObject, propertyIdRef, &shouldContinueRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    bool shouldContinue = true;
    errorCode = JsBooleanToBool(shouldContinueRef, &shouldContinue);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueType resultType;
    errorCode = JsGetValueType(result, &resultType);
    CHAKRA_VERIFY(errorCode == JsNoError);

    if (resultType == JsString) {
      v8::Local<v8::String> v8Str = MessageImpl::JsValueRefToV8String(result);
      v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
      Debug::handler(*msgImpl);
    }

    return shouldContinue;
  }

  void MessageQueue::ProcessDebuggerMessage() {
    if (this->isProcessingDebuggerMsg) {
      return;
    }

    if (!this->IsEmpty()) {
      this->isProcessingDebuggerMsg = true;

      wchar_t* msg = this->PopMessage();

      JsValueRef msgArg;
      JsErrorCode errorCode = JsPointerToString(msg, wcslen(msg), &msgArg);
      CHAKRA_VERIFY(errorCode == JsNoError);

      JsValueRef args[] = { jsrt::GetUndefined(), msgArg };
      JsValueRef responseRef;
      errorCode = JsCallFunction(
          this->processDebugProtocolJSON, args, _countof(args), &responseRef);
      CHAKRA_VERIFY(errorCode == JsNoError);

      JsValueType resultType;
      errorCode = JsGetValueType(responseRef, &resultType);
      CHAKRA_VERIFY(errorCode == JsNoError);

      if (resultType == JsString) {
        v8::Local<v8::String> v8Str =
            MessageImpl::JsValueRefToV8String(responseRef);
        v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
        Debug::handler(*msgImpl);
      }

      this->isProcessingDebuggerMsg = false;
    }
  }

  bool MessageQueue::ConvertMessage(
      JsDiagDebugEvent debugEvent,
      JsValueRef eventData) {
    JsValueRef debugEventRef;
    JsIntToNumber(debugEvent, &debugEventRef);

    JsValueRef args[] = { jsrt::GetUndefined(), debugEventRef, eventData };
    JsValueRef result;
    JsErrorCode errorCode = JsCallFunction(
        this->processJsrtEventData, args, _countof(args), &result);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsPropertyIdRef propertyIdRef;
    JsGetPropertyIdFromName(L"shouldContinue", &propertyIdRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef shouldContinueRef;
    errorCode = JsGetProperty(
        this->chakraDebugObject, propertyIdRef, &shouldContinueRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    bool shouldContinue = true;
    errorCode = JsBooleanToBool(shouldContinueRef, &shouldContinue);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueType resultType;
    errorCode = JsGetValueType(result, &resultType);
    CHAKRA_VERIFY(errorCode == JsNoError);

    if (resultType == JsString) {
      v8::Local<v8::String> v8Str = MessageImpl::JsValueRefToV8String(result);
      v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
      Debug::handler(*msgImpl);
    }
    return shouldContinue;
  }

  bool MessageQueue::ProcessJsrtDebugEvent(
      JsDiagDebugEvent debugEvent,
      JsValueRef eventData) {
    JsValueRef debugEventRef;
    JsErrorCode errorCode = JsIntToNumber(debugEvent, &debugEventRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef args[] = { jsrt::GetUndefined(), debugEventRef, eventData };
    JsValueRef result;
    errorCode = JsCallFunction(
        this->processJsrtEventData, args, _countof(args), &result);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueType resultType;
    errorCode = JsGetValueType(result, &resultType);
    CHAKRA_VERIFY(errorCode == JsNoError);

    if (resultType == JsString) {
      v8::Local<v8::String> v8Str = MessageImpl::JsValueRefToV8String(result);
      v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
      Debug::handler(*msgImpl);
    }

    return this->ShouldContinue();
  }

  bool MessageQueue::ShouldContinue() {
    JsPropertyIdRef propertyIdRef;
    JsErrorCode errorCode = JsGetPropertyIdFromName(
        L"shouldContinue", &propertyIdRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef shouldContinueRef;
    errorCode = JsGetProperty(
        this->chakraDebugObject, propertyIdRef, &shouldContinueRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    bool shouldContinue = true;
    errorCode = JsBooleanToBool(shouldContinueRef, &shouldContinue);
    CHAKRA_VERIFY(errorCode == JsNoError);

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
    const wchar_t *script;

    JsErrorCode errorCode = jsrt::ToString(result, &scriptRef, &script);
    CHAKRA_VERIFY(errorCode == JsNoError);

    v8::Local<v8::String> v8Str = v8::String::NewFromTwoByte(
        jsrt::IsolateShim::GetCurrentAsIsolate(), (uint16_t*)script);
    return v8Str;
  }

  Local<String> MessageImpl::GetJSON() const {
    return this->v8Str;
  }

  bool Debug::EnableAgent(
      const char *name,
      int port,
      bool wait_for_connection) {
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
    if (g_debugContext == JS_INVALID_REFERENCE) {
      HandleScope scope(isolate);
      g_debugContext = *Context::New(isolate);
      JsAddRef(g_debugContext, nullptr);

      Local<Object> global = Context::GetCurrent()->Global();

      // CHAKRA-TODO: Chakra doesn't fully implement the debugger without
      // --debug flag. Add a dummy 'Debug' on global object if it doesn't
      // already exist.
      {
        Context::Scope context_scope(g_debugContext);
        jsrt::ContextShim* contextShim = jsrt::ContextShim::GetCurrent();
        JsValueRef ensureDebug = contextShim->GetensureDebugFunction();
        JsValueRef unused;
        if (jsrt::CallFunction(ensureDebug, *global, &unused) != JsNoError) {
          return Local<Context>();
        }
      }
    }

    return static_cast<Context*>(g_debugContext);
  }

  void Debug::Dispose() {
    if (g_EnableDebug) {
      CoUninitialize();
    }
  }

  void __stdcall JsDiagDebugEventHandler(
      JsDiagDebugEvent debugEvent,
      JsValueRef eventData,
      void* callbackState) {
    if (Debug::handler == nullptr) {
      return;
    }

    bool shouldContinue = true;
    bool isMsgToProcess = !Debug::messageQueue.IsEmpty();

    do {
      while (!Debug::messageQueue.isProcessingDebuggerMsg && isMsgToProcess) {
        Debug::messageQueue.ProcessDebuggerMessage();
        isMsgToProcess = !Debug::messageQueue.IsEmpty();
      }

      if (eventData != nullptr) {
        Debug::messageQueue.ProcessJsrtDebugEvent(debugEvent, eventData);
        eventData = nullptr;
      }

      shouldContinue = Debug::messageQueue.ShouldContinue();

      isMsgToProcess = !Debug::messageQueue.IsEmpty();

      while (!shouldContinue && !isMsgToProcess &&
          !Debug::messageQueue.isProcessingDebuggerMsg) {
        Debug::messageQueue.WaitForMessage();
        isMsgToProcess = !Debug::messageQueue.IsEmpty();
      }

      isMsgToProcess = !Debug::messageQueue.IsEmpty();
    } while (isMsgToProcess &&
        !Debug::messageQueue.isProcessingDebuggerMsg && !shouldContinue);
  }

  void Debug::StartDebugging(JsRuntimeHandle runtime) {
    JsErrorCode errorCode = JsDiagStartDebugging(
        runtime, JsDiagDebugEventHandler, nullptr);
    CHAKRA_VERIFY(errorCode == JsNoError);
  }

  void Debug::SendCommand(Isolate* isolate,
    const uint16_t* command, int length,
    ClientData* client_data) {
    CHAKRA_ASSERT(client_data == NULL);

    // Request async break from engine so that we can process
    // commands on JsDiagDebugEventAsyncBreak
    JsErrorCode errorCode = JsDiagRequestAsyncBreak(
        jsrt::IsolateShim::FromIsolate(isolate)->GetRuntimeHandle());
    CHAKRA_VERIFY(errorCode == JsNoError);

    // Save command in a queue and when JsDiagDebugEventHandler
    // is called we need to process the queue
    messageQueue.SaveMessage(command, length);
  }

  static JsValueRef __stdcall Log(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueRef scriptRef;
    const wchar_t *script;

    if (argumentCount > 1 &&
      jsrt::ToString(arguments[1], &scriptRef, &script) == JsNoError) {
      wprintf(L"%s\n", script);
#ifdef DEBUG
      flushall();
#endif
    }

    return JS_INVALID_REFERENCE;
  }

  static JsValueRef __stdcall SendDelayedRespose(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueType resultType;
    JsErrorCode errorCode = JsGetValueType(arguments[1], &resultType);
    CHAKRA_VERIFY(errorCode == JsNoError);

    if (resultType == JsString) {
      v8::Local<v8::String> v8Str = MessageImpl::JsValueRefToV8String(
          arguments[1]);
      v8::MessageImpl* msgImpl = new v8::MessageImpl(v8Str);
      Debug::handler(*msgImpl);
    }

    return JS_INVALID_REFERENCE;
  }

  static JsValueRef __stdcall JsGetScripts(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueRef sourcesList;
    JsErrorCode errorCode = JsDiagGetScripts(&sourcesList);
    CHAKRA_VERIFY(errorCode == JsNoError);

    return sourcesList;
  }

  static JsValueRef __stdcall JsGetSource(
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
      CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return source;
  }

  static JsValueRef __stdcall JsSetStepType(
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
      CHAKRA_VERIFY(errorCode == JsNoError);
      success = true;
    }

    JsValueRef returnRef = JS_INVALID_REFERENCE;
    JsBoolToBoolean(success, &returnRef);

    return returnRef;
  }

  static JsValueRef __stdcall JsSetBreakpoint(
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
      CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return bpObject;
  }

  static JsValueRef __stdcall JsGetFunctionPosition(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueRef valueRef = JS_INVALID_REFERENCE;
    JsErrorCode errorCode = JsConvertValueToObject(arguments[1], &valueRef);
    CHAKRA_VERIFY(errorCode == JsNoError);
    JsValueRef funcInfo = JS_INVALID_REFERENCE;
    errorCode = JsDiagGetFunctionPosition(valueRef, &funcInfo);
    CHAKRA_VERIFY(errorCode == JsNoError);

    return funcInfo;
  }

  static JsValueRef __stdcall JsGetStackTrace(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueRef stackInfo = JS_INVALID_REFERENCE;
    JsErrorCode errorCode = JsDiagGetStackTrace(&stackInfo);
    CHAKRA_VERIFY(errorCode == JsNoError);

    return stackInfo;
  }

  static JsValueRef __stdcall JsGetStackProperties(
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
      CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return properties;
  }

  static JsValueRef __stdcall JsGetObjectFromHandle(
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
        CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return properties;
  }

  static JsValueRef __stdcall JsGetBreakpoints(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueRef properties = JS_INVALID_REFERENCE;
    JsErrorCode errorCode = JsDiagGetBreakpoints(&properties);
    CHAKRA_VERIFY(errorCode == JsNoError);

    return properties;
  }

  static JsValueRef __stdcall JsRemoveBreakpoint(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    int bpId;
    if (argumentCount > 1 &&
      jsrt::ValueToInt(arguments[1], &bpId) == JsNoError) {
      JsErrorCode errorCode = JsDiagRemoveBreakpoint(bpId);
      CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return JS_INVALID_REFERENCE;
  }

  static JsValueRef __stdcall JsGetProperties(
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
        CHAKRA_VERIFY(errorCode == JsNoError);
    }
    return properties;
  }

  static JsValueRef __stdcall JsEvaluate(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    int frameIndex;
    JsValueRef scriptRef;
    const wchar_t *script;
    JsValueRef result = JS_INVALID_REFERENCE;

    if (argumentCount > 2 &&
      jsrt::ValueToInt(arguments[2], &frameIndex) == JsNoError &&
      jsrt::ToString(arguments[1], &scriptRef, &script) == JsNoError) {
      JsErrorCode errorCode = JsDiagEvaluate(script, frameIndex, &result);
      if (errorCode != JsNoError) {
          jsrt::Fatal("internal error %s(%d): %d", __FILE__, __LINE__, errorCode);
      }
      CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return result;
  }

  static JsValueRef __stdcall JsEvaluateScript(
      JsValueRef callee,
      bool isConstructCall,
      JsValueRef *arguments,
      unsigned short argumentCount,
      void *callbackState) {
    JsValueRef scriptRef;
    const wchar_t *script;
    JsValueRef result = JS_INVALID_REFERENCE;

    if (argumentCount > 1 &&
      jsrt::ToString(arguments[1], &scriptRef, &script) == JsNoError) {
      JsErrorCode errorCode = JsRunScript(
          script, JS_SOURCE_CONTEXT_NONE, L"", &result);
      if (errorCode != JsNoError) {
          jsrt::Fatal("internal error %s(%d): %d", __FILE__, __LINE__, errorCode);
      }
      CHAKRA_VERIFY(errorCode == JsNoError);
    }

    return result;
  }

  static JsValueRef __stdcall JsSetBreakOnException(
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

  static JsValueRef __stdcall JsGetBreakOnException(
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
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef returnRef = JS_INVALID_REFERENCE;
    errorCode = JsIntToNumber(breakOnExceptionAttributes, &returnRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    return returnRef;
  }

  void Debug::InstallHostCallback(JsValueRef chakraDebugObject,
      const wchar_t *name, JsNativeFunction nativeFunction) {
    JsValueRef nameVar;
    JsErrorCode errorCode = JsPointerToString(name, wcslen(name), &nameVar);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef funcRef;
    errorCode = JsCreateNamedFunction(
        nameVar, nativeFunction, nullptr, &funcRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsPropertyIdRef propertyIdRef;
    errorCode = JsGetPropertyIdFromName(name, &propertyIdRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    errorCode = JsSetProperty(chakraDebugObject, propertyIdRef, funcRef, true);
    CHAKRA_VERIFY(errorCode == JsNoError);
  }

  void Debug::SetChakraDebugObject(JsValueRef chakraDebugObject) {
    JsPropertyIdRef propertyIdRef;
    JsErrorCode errorCode = JsGetPropertyIdFromName(
        L"ProcessDebugProtocolJSON", &propertyIdRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef processDebugProtocolJSON;
    errorCode = JsGetProperty(chakraDebugObject,
        propertyIdRef, &processDebugProtocolJSON);
    CHAKRA_VERIFY(errorCode == JsNoError);

    errorCode = JsGetPropertyIdFromName(L"ProcessJsrtEventData",
        &propertyIdRef);
    CHAKRA_VERIFY(errorCode == JsNoError);

    JsValueRef processJsrtEventData;
    errorCode = JsGetProperty(chakraDebugObject,
        propertyIdRef, &processJsrtEventData);
    CHAKRA_VERIFY(errorCode == JsNoError);

    Debug::messageQueue.SetProcessMessage(chakraDebugObject,
        processDebugProtocolJSON, processJsrtEventData);

    Debug::InstallHostCallback(chakraDebugObject,
        L"log", Log);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetScripts", JsGetScripts);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetSource", JsGetSource);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagSetStepType", JsSetStepType);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagSetBreakpoint", JsSetBreakpoint);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetFunctionPosition", JsGetFunctionPosition);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetStackTrace", JsGetStackTrace);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetStackProperties", JsGetStackProperties);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetObjectFromHandle", JsGetObjectFromHandle);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagEvaluateScript", JsEvaluateScript);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagEvaluate", JsEvaluate);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetBreakpoints", JsGetBreakpoints);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetProperties", JsGetProperties);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagRemoveBreakpoint", JsRemoveBreakpoint);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagSetBreakOnException", JsSetBreakOnException);
    Debug::InstallHostCallback(chakraDebugObject,
        L"JsDiagGetBreakOnException", JsGetBreakOnException);
    Debug::InstallHostCallback(chakraDebugObject,
        L"SendDelayedRespose", SendDelayedRespose);
  }

  void Debug::SetMessageHandler(Isolate* isolate, MessageHandler handler) {
    Debug::handler = handler;
  }

}  // namespace v8
