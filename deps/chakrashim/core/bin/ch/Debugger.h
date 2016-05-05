//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

static const WCHAR controllerScript[] = {
#include "DbgController.js.encoded"
    _u('\0')
};

class Debugger
{
public:
    static Debugger* GetDebugger(JsRuntimeHandle runtime);
    static void CloseDebugger();
    static JsRuntimeHandle GetRuntime();
    bool StartDebugging(JsRuntimeHandle runtime);
    bool StopDebugging(JsRuntimeHandle runtime);
    bool HandleDebugEvent(JsDiagDebugEvent debugEvent, JsValueRef eventData);
    bool CompareOrWriteBaselineFile(LPCWSTR fileName);
    bool SourceRunDown();
    bool DumpFunctionInfo(JsValueRef functionInfo);
    bool IsDetached() const { return isDetached; }
private:
    Debugger(JsRuntimeHandle runtime);
    ~Debugger();
    bool Initialize();
    JsRuntimeHandle m_runtime;
    JsContextRef m_context;
    bool isDetached;
    bool InstallDebugCallbacks(JsValueRef hostDebugObject);
    bool SetBaseline();
    bool SetInspectMaxStringLength();
    bool CallFunction(char16 const * functionName, JsValueRef *result, JsValueRef arg1 = JS_INVALID_REFERENCE, JsValueRef arg2 = JS_INVALID_REFERENCE);
    bool CallFunctionNoResult(char16 const * functionName, JsValueRef arg1 = JS_INVALID_REFERENCE, JsValueRef arg2 = JS_INVALID_REFERENCE);
public:
    static void CALLBACK JsDiagDebugEventHandler(_In_ JsDiagDebugEvent debugEvent, _In_ JsValueRef eventData, _In_opt_ void* callbackState);
    static JsValueRef CALLBACK JsDiagGetSource(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagSetBreakpoint(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetStackTrace(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetBreakpoints(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagRemoveBreakpoint(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagSetBreakOnException(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetBreakOnException(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagSetStepType(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetScripts(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetStackProperties(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetProperties(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagGetObjectFromHandle(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);
    static JsValueRef CALLBACK JsDiagEvaluate(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState);

    static Debugger* debugger;
};
