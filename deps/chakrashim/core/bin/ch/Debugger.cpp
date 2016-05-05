//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "stdafx.h"

#define MAX_BASELINE_SIZE       (1024*1024*200)

void CHAKRA_CALLBACK Debugger::JsDiagDebugEventHandler(_In_ JsDiagDebugEvent debugEvent, _In_ JsValueRef eventData, _In_opt_ void* callbackState)
{
    Debugger* debugger = (Debugger*)callbackState;
    debugger->HandleDebugEvent(debugEvent, eventData);
}

JsValueRef Debugger::JsDiagGetSource(JsValueRef callee, bool isConstructCall, JsValueRef * arguments, unsigned short argumentCount, void * callbackState)
{
    int scriptId;
    JsValueRef source = JS_INVALID_REFERENCE;

    if (argumentCount > 1)
    {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &scriptId));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetSource(scriptId, &source));
    }

    return source;
}

JsValueRef Debugger::JsDiagSetBreakpoint(JsValueRef callee, bool isConstructCall, JsValueRef * arguments, unsigned short argumentCount, void * callbackState)
{
    int scriptId;
    int line;
    int column;
    JsValueRef bpObject = JS_INVALID_REFERENCE;

    if (argumentCount > 3)
    {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &scriptId));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[2], &line));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[3], &column));

        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagSetBreakpoint(scriptId, line, column, &bpObject));
    }

    return bpObject;
}

JsValueRef Debugger::JsDiagGetStackTrace(JsValueRef callee, bool isConstructCall, JsValueRef * arguments, unsigned short argumentCount, void * callbackState)
{
    JsValueRef stackInfo = JS_INVALID_REFERENCE;
    IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetStackTrace(&stackInfo));
    return stackInfo;
}

JsValueRef Debugger::JsDiagGetBreakpoints(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    JsValueRef breakpoints = JS_INVALID_REFERENCE;
    IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetBreakpoints(&breakpoints));
    return breakpoints;
}

JsValueRef Debugger::JsDiagRemoveBreakpoint(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    int bpId;
    if (argumentCount > 1)
    {
        JsValueRef numberValue;
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsConvertValueToNumber(arguments[1], &numberValue));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(numberValue, &bpId));

        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagRemoveBreakpoint(bpId));
    }

    return JS_INVALID_REFERENCE;
}

JsValueRef Debugger::JsDiagSetBreakOnException(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    int exceptionAttributes;

    if (argumentCount > 1)
    {
        JsValueRef breakOnException;
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsConvertValueToNumber(arguments[1], &breakOnException));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(breakOnException, &exceptionAttributes));

        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagSetBreakOnException(Debugger::GetRuntime(), (JsDiagBreakOnExceptionAttributes)exceptionAttributes));
    }

    return JS_INVALID_REFERENCE;
}

JsValueRef Debugger::JsDiagGetBreakOnException(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    JsDiagBreakOnExceptionAttributes exceptionAttributes;
    IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetBreakOnException(Debugger::GetRuntime(), &exceptionAttributes));

    JsValueRef exceptionAttributesRef;
    IfJsErrorFailLogAndRet(ChakraRTInterface::JsDoubleToNumber((double)exceptionAttributes, &exceptionAttributesRef));
    return exceptionAttributesRef;
}

JsValueRef Debugger::JsDiagSetStepType(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    int stepType;

    if (argumentCount > 1)
    {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &stepType));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagSetStepType((JsDiagStepType)stepType));
    }

    return JS_INVALID_REFERENCE;
}

JsValueRef Debugger::JsDiagGetScripts(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    JsValueRef sourcesList = JS_INVALID_REFERENCE;
    IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetScripts(&sourcesList));
    return sourcesList;
}

JsValueRef Debugger::JsDiagGetStackProperties(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    JsValueRef properties = JS_INVALID_REFERENCE;
    int stackFrameIndex;

    if (argumentCount > 1)
    {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &stackFrameIndex));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetStackProperties(stackFrameIndex, &properties));
    }

    return properties;
}

JsValueRef Debugger::JsDiagGetProperties(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    JsValueRef properties = JS_INVALID_REFERENCE;
    int objectHandle;
    int fromCount;
    int totalCount;

    if (argumentCount > 3)
    {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &objectHandle));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[2], &fromCount));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[3], &totalCount));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetProperties(objectHandle, fromCount, totalCount, &properties));
    }

    return properties;
}

JsValueRef Debugger::JsDiagGetObjectFromHandle(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    JsValueRef properties = JS_INVALID_REFERENCE;
    int objectHandle;

    if (argumentCount > 1)
    {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &objectHandle));
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagGetObjectFromHandle(objectHandle, &properties));
    }

    return properties;
}

JsValueRef Debugger::JsDiagEvaluate(JsValueRef callee, bool isConstructCall, JsValueRef *arguments, unsigned short argumentCount, void *callbackState)
{
    int stackFrameIndex;
    JsValueRef result = JS_INVALID_REFERENCE;

    if (argumentCount > 2) {
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsNumberToInt(arguments[1], &stackFrameIndex));

        LPCWSTR str = nullptr;
        size_t length;
        IfJsErrorFailLogAndRet(ChakraRTInterface::JsValueToWchar(arguments[2], &str, &length));

        IfJsErrorFailLogAndRet(ChakraRTInterface::JsDiagEvaluate(str, stackFrameIndex, &result));
    }

    return result;
}

Debugger::Debugger(JsRuntimeHandle runtime)
{
    this->m_runtime = runtime;
    this->m_context = JS_INVALID_REFERENCE;
    this->isDetached = true;
}

Debugger::~Debugger()
{
    this->m_runtime = JS_INVALID_RUNTIME_HANDLE;
}

Debugger * Debugger::GetDebugger(JsRuntimeHandle runtime)
{
    if (Debugger::debugger == nullptr)
    {
        Debugger::debugger = new Debugger(runtime);
        Debugger::debugger->Initialize();
    }

    return Debugger::debugger;
}

void Debugger::CloseDebugger()
{
    if (Debugger::debugger != nullptr)
    {
        delete Debugger::debugger;
        Debugger::debugger = nullptr;
    }
}

JsRuntimeHandle Debugger::GetRuntime()
{
    return Debugger::debugger != nullptr ? Debugger::debugger->m_runtime : JS_INVALID_RUNTIME_HANDLE;
}

bool Debugger::Initialize()
{
    // Create a new context and run dbgcontroller.js in that context
    // setup dbgcontroller.js callbacks

    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsCreateContext(this->m_runtime, &this->m_context));

    AutoRestoreContext autoRestoreContext(this->m_context);

    JsValueRef globalFunc = JS_INVALID_REFERENCE;

    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsParseScriptWithAttributes(controllerScript, JS_SOURCE_CONTEXT_NONE, _u("DbgController.js"), JsParseScriptAttributeLibraryCode, &globalFunc));

    JsValueRef undefinedValue;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetUndefinedValue(&undefinedValue));

    JsValueRef args[] = { undefinedValue };
    JsValueRef result = JS_INVALID_REFERENCE;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsCallFunction(globalFunc, args, _countof(args), &result));

    JsValueRef globalObj = JS_INVALID_REFERENCE;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetGlobalObject(&globalObj));

    JsPropertyIdRef hostDebugObjectPropId;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetPropertyIdFromName(_u("hostDebugObject"), &hostDebugObjectPropId));

    JsPropertyIdRef hostDebugObject;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetProperty(globalObj, hostDebugObjectPropId, &hostDebugObject));

    this->InstallDebugCallbacks(hostDebugObject);

    if (!WScriptJsrt::Initialize())
    {
        return false;
    }

    if (HostConfigFlags::flags.dbgbaselineIsEnabled)
    {
        this->SetBaseline();
    }

    if (HostConfigFlags::flags.InspectMaxStringLengthIsEnabled)
    {
        this->SetInspectMaxStringLength();
    }

    return true;
}

bool Debugger::InstallDebugCallbacks(JsValueRef hostDebugObject)
{
    HRESULT hr = S_OK;
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetSource"), Debugger::JsDiagGetSource));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagSetBreakpoint"), Debugger::JsDiagSetBreakpoint));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetStackTrace"), Debugger::JsDiagGetStackTrace));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetBreakpoints"), Debugger::JsDiagGetBreakpoints));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagRemoveBreakpoint"), Debugger::JsDiagRemoveBreakpoint));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagSetBreakOnException"), Debugger::JsDiagSetBreakOnException));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetBreakOnException"), Debugger::JsDiagGetBreakOnException));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagSetStepType"), Debugger::JsDiagSetStepType));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetScripts"), Debugger::JsDiagGetScripts));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetStackProperties"), Debugger::JsDiagGetStackProperties));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetProperties"), Debugger::JsDiagGetProperties));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagGetObjectFromHandle"), Debugger::JsDiagGetObjectFromHandle));
    IfFalseGo(WScriptJsrt::InstallObjectsOnObject(hostDebugObject, _u("JsDiagEvaluate"), Debugger::JsDiagEvaluate));
Error:
    return hr != S_OK;
}

bool Debugger::SetBaseline()
{
    LPSTR script = nullptr;
    FILE *file = nullptr;
    int numChars = 0;
    LPWSTR wideScript = nullptr;
    HRESULT hr = S_OK;

    if (_wfopen_s(&file, HostConfigFlags::flags.dbgbaseline, _u("rb")) != 0)
    {
        Helpers::LogError(_u("opening baseline file '%s'"), HostConfigFlags::flags.dbgbaseline);
    }
    else
    {
        int fileSize = _filelength(_fileno(file));
        if (fileSize <= MAX_BASELINE_SIZE)
        {
            script = new char[fileSize + 1];

            numChars = static_cast<int>(fread(script, sizeof(script[0]), fileSize, file));
            if (numChars == fileSize)
            {
                script[numChars] = '\0';

                // Convert to wide for sending to script.
                wideScript = new char16[numChars + 2];
                if (MultiByteToWideChar(CP_UTF8, 0, script, static_cast<int>(strlen(script)) + 1, wideScript, numChars + 2) == 0)
                {
                    Helpers::LogError(_u("MultiByteToWideChar"));
                    IfFailGo(E_FAIL);
                }

                JsValueRef wideScriptRef;
                IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsPointerToString(wideScript, wcslen(wideScript), &wideScriptRef));

                this->CallFunctionNoResult(_u("SetBaseline"), wideScriptRef);
            }
            else
            {
                Helpers::LogError(_u("failed to read from baseline file"));
                IfFailGo(E_FAIL);
            }
        }
        else
        {
            Helpers::LogError(_u("baseline file too large"));
            IfFailGo(E_FAIL);
        }
    }
Error:
    if (script)
        delete[] script;
    if (wideScript)
        delete[] wideScript;
    if (file)
        fclose(file);
    return hr == S_OK;
}

bool Debugger::SetInspectMaxStringLength()
{
    Assert(HostConfigFlags::flags.InspectMaxStringLength > 0);

    JsValueRef maxStringRef;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsDoubleToNumber(HostConfigFlags::flags.InspectMaxStringLength, &maxStringRef));

    return this->CallFunctionNoResult(_u("SetInspectMaxStringLength"), maxStringRef);
}

bool Debugger::CallFunction(char16 const * functionName, JsValueRef *result, JsValueRef arg1, JsValueRef arg2)
{
    AutoRestoreContext autoRestoreContext(this->m_context);

    // Get the global object
    JsValueRef globalObj = JS_INVALID_REFERENCE;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetGlobalObject(&globalObj));

    // Get a script string for the function name
    JsPropertyIdRef targetFuncId = JS_INVALID_REFERENCE;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetPropertyIdFromName(functionName, &targetFuncId));

    // Get the target function
    JsValueRef targetFunc = JS_INVALID_REFERENCE;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetProperty(globalObj, targetFuncId, &targetFunc));

    static const unsigned short MaxArgs = 2;
    JsValueRef args[MaxArgs + 1];

    // Pass in undefined for 'this'
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetUndefinedValue(&args[0]));

    unsigned short argCount = 1;

    if (arg1 != JS_INVALID_REFERENCE)
    {
        args[argCount++] = arg1;
    }

    Assert(arg2 == JS_INVALID_REFERENCE || argCount != 1);

    if (arg2 != JS_INVALID_REFERENCE)
    {
        args[argCount++] = arg2;
    }

    // Call the function
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsCallFunction(targetFunc, args, argCount, result));

    return true;
}

bool Debugger::CallFunctionNoResult(char16 const * functionName, JsValueRef arg1, JsValueRef arg2)
{
    JsValueRef result = JS_INVALID_REFERENCE;
    return this->CallFunction(functionName, &result, arg1, arg2);
}

bool Debugger::DumpFunctionInfo(JsValueRef functionInfo)
{
    return this->CallFunctionNoResult(_u("DumpFunctionInfo"), functionInfo);
}

bool Debugger::StartDebugging(JsRuntimeHandle runtime)
{
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsDiagStartDebugging(runtime, Debugger::JsDiagDebugEventHandler, this));

    this->isDetached = false;

    return true;
}

bool Debugger::StopDebugging(JsRuntimeHandle runtime)
{
    void* callbackState = nullptr;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsDiagStopDebugging(runtime, &callbackState));

    Assert(callbackState == this);

    this->isDetached = true;

    return true;
}

bool Debugger::HandleDebugEvent(JsDiagDebugEvent debugEvent, JsValueRef eventData)
{
    JsValueRef debugEventRef;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsDoubleToNumber(debugEvent, &debugEventRef));

    return this->CallFunctionNoResult(_u("HandleDebugEvent"), debugEventRef, eventData);
}

bool Debugger::CompareOrWriteBaselineFile(LPCWSTR fileName)
{
    AutoRestoreContext autoRestoreContext(this->m_context);

    // Pass in undefined for 'this'
    JsValueRef undefinedRef;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsGetUndefinedValue(&undefinedRef));

    JsValueRef result = JS_INVALID_REFERENCE;

    bool testPassed = false;

    if (HostConfigFlags::flags.dbgbaselineIsEnabled)
    {
        this->CallFunction(_u("Verify"), &result);
        JsValueRef numberVal;
        IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsConvertValueToNumber(result, &numberVal));
        int val;
        IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsNumberToInt(numberVal, &val));
        testPassed = (val == 0);
    }

    if (!testPassed)
    {
        this->CallFunction(_u("GetOutputJson"), &result);

        LPCWSTR baselineData;
        size_t baselineDataLength;
        IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsStringToPointer(result, &baselineData, &baselineDataLength));

#pragma warning(disable: 38021) // From MSDN: For the code page UTF-8 dwFlags must be set to either 0 or WC_ERR_INVALID_CHARS. Otherwise, the function fails with ERROR_INVALID_FLAGS.
        int multiByteDataLength = WideCharToMultiByte(CP_UTF8, 0, baselineData, -1, NULL, 0, NULL, NULL);
        LPSTR baselineDataANSI = new char[multiByteDataLength];
        if (WideCharToMultiByte(CP_UTF8, 0, baselineData, -1, baselineDataANSI, multiByteDataLength, NULL, NULL) == 0)
        {
#pragma warning(default: 38021) // From MSDN: For the code page UTF-8 dwFlags must be set to either 0 or WC_ERR_INVALID_CHARS. Otherwise, the function fails with ERROR_INVALID_FLAGS.
            return false;
        }

        wchar_t baselineFilename[256];
        swprintf_s(baselineFilename, _countof(baselineFilename), HostConfigFlags::flags.dbgbaselineIsEnabled ? _u("%s.dbg.baseline.rebase") : _u("%s.dbg.baseline"), fileName);

        FILE *file = nullptr;
        if (_wfopen_s(&file, baselineFilename, _u("wt")) != 0)
        {
            return false;
        }

        int countWritten = static_cast<int>(fwrite(baselineDataANSI, sizeof(baselineDataANSI[0]), strlen(baselineDataANSI), file));
        if (countWritten != (int)strlen(baselineDataANSI))
        {
            Assert(false);
            return false;
        }

        fclose(file);

        if (!HostConfigFlags::flags.dbgbaselineIsEnabled)
        {
            wprintf(_u("No baseline file specified, baseline created at '%s'\n"), baselineFilename);
        }
        else
        {
            Helpers::LogError(_u("Rebase file created at '%s'\n"), baselineFilename);
        }
    }

    return true;
}

bool Debugger::SourceRunDown()
{
    AutoRestoreContext autoRestoreContext(this->m_context);

    JsValueRef sourcesList = JS_INVALID_REFERENCE;
    IfJsrtErrorFailLogAndRetFalse(ChakraRTInterface::JsDiagGetScripts(&sourcesList));

    return this->CallFunctionNoResult(_u("HandleSourceRunDown"), sourcesList);
}
