//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Wasm
{
    class WasmModule;
};

namespace Js
{
    class WasmLibrary
    {
    public:
        static JavascriptMethod WasmDeferredParseEntryPoint(AsmJsScriptFunction** funcPtr, int internalCall);
#ifdef ENABLE_WASM
        class EntryInfo
        {
        public:
            static FunctionInfo instantiateModule;
        };

        static Var instantiateModule(RecyclableObject* function, CallInfo callInfo, ...);
        static const unsigned int experimentalVersion;

        static Var WasmLazyTrapCallback(RecyclableObject *callee, CallInfo, ...);
        static Var WasmDeferredParseInternalThunk(RecyclableObject* function, CallInfo callInfo, ...);
        static Var WasmDeferredParseExternalThunk(RecyclableObject* function, CallInfo callInfo, ...);
    private:
        static void WasmFunctionGenerateBytecode(AsmJsScriptFunction* func, bool propagateError);
        static void WasmLoadDataSegs(Wasm::WasmModule * wasmModule, Var* heap, ScriptContext* ctx);
        static void WasmLoadFunctions(Wasm::WasmModule * wasmModule, ScriptContext* ctx, Var* moduleMemoryPtr, Var* exportObj, Var* localModuleFunctions, bool* hasAnyLazyTraps);
        static void WasmBuildObject(Wasm::WasmModule * wasmModule, ScriptContext* ctx, Var exportsNamespace, Var* heap, Var* exportObj, bool* hasAnyLazyTraps, Var* localModuleFunctions, Var* importFunctions);
        static void WasmLoadImports(Wasm::WasmModule * wasmModule, ScriptContext* ctx, Var* importFunctions, Var moduleEnv, Var ffi);
        static void WasmLoadGlobals(Wasm::WasmModule * wasmModule, ScriptContext* ctx, Var moduleEnv, Var ffi);
        static void WasmLoadIndirectFunctionTables(Wasm::WasmModule * wasmModule, ScriptContext* ctx, Var** indirectFunctionTables, Var* localModuleFunctions, Var* importFunctions);
        static Var GetFunctionObjFromFunctionIndex(Wasm::WasmModule * wasmModule, ScriptContext* ctx, uint32 funcIndex, Var* localModuleFunctions, Var* importFunctions);

        static Var LoadWasmScript(
            ScriptContext* scriptContext,
            const char16* script,
            SRCINFO const * pSrcInfo,
            CompileScriptException * pse,
            Utf8SourceInfo** ppSourceInfo,
            const uint lengthBytes,
            const char16 *rootDisplayName,
            Js::Var ffi,
            Js::Var* start = nullptr
        );
        static char16* lastWasmExceptionMessage;
#endif
    };
}