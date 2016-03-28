//---------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#include "JsrtPch.h"
#include "JsrtDebuggerObject.h"
#include "JsrtDebugUtils.h"
#include "JsrtDebug.h"

DebuggerObjectBase::DebuggerObjectBase(DebuggerObjectType type, DebuggerObjectsManager* debuggerObjectsManager) :
    type(type),
    debuggerObjectsManager(debuggerObjectsManager)
{
    Assert(debuggerObjectsManager != nullptr);
    this->handle = debuggerObjectsManager->GetNextHandle();
}

DebuggerObjectBase::~DebuggerObjectBase()
{
    this->debuggerObjectsManager = nullptr;
}

DebuggerObjectsManager * DebuggerObjectBase::GetDebuggerObjectsManager()
{
    return this->debuggerObjectsManager;
}

Js::DynamicObject * DebuggerObjectBase::GetChildrens(Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{
    Assert("Wrong type for GetChildrens");
    return nullptr;
}

Js::DynamicObject * DebuggerObjectBase::GetChildrens(WeakArenaReference<Js::IDiagObjectModelWalkerBase>* walkerRef, Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{
    Js::DynamicObject* childrensObject = scriptContext->GetLibrary()->CreateObject();

    uint propertiesArrayCount = 0;
    Js::JavascriptArray* propertiesArray = scriptContext->GetLibrary()->CreateArray();

    uint debuggerOnlyPropertiesArrayCount = 0;
    Js::JavascriptArray* debuggerOnlyPropertiesArray = scriptContext->GetLibrary()->CreateArray();

    Js::IDiagObjectModelWalkerBase* walker = walkerRef->GetStrongReference();

    ulong childrensCount = 0;

    if (walker != nullptr)
    {
        try
        {
            childrensCount = walker->GetChildrenCount();
        }
        catch (Js::JavascriptExceptionObject*) {}

        if (fromCount < childrensCount)
        {
            for (ulong i = fromCount; i < childrensCount && (propertiesArrayCount + debuggerOnlyPropertiesArrayCount) < totalCount; ++i)
            {
                Js::ResolvedObject resolvedObject;

                try
                {
                    walker->Get(i, &resolvedObject);
                }
                catch (Js::JavascriptExceptionObject* exception)
                {
                    Js::Var error = exception->GetThrownObject(scriptContext);
                    resolvedObject.obj = error;
                    resolvedObject.address = NULL;
                    resolvedObject.scriptContext = exception->GetScriptContext();
                    resolvedObject.typeId = Js::JavascriptOperators::GetTypeId(error);
                    resolvedObject.name = _u("{error}");
                    resolvedObject.propId = Js::Constants::NoProperty;
                }

                AutoPtr<WeakArenaReference<Js::IDiagObjectModelDisplay>> objectDisplayWeakRef = resolvedObject.GetObjectDisplay();
                Js::IDiagObjectModelDisplay* resolvedObjectDisplay = objectDisplayWeakRef->GetStrongReference();
                if (resolvedObjectDisplay != nullptr)
                {
                    DebuggerObjectBase* debuggerObject = DebuggerObjectProperty::Make(this->GetDebuggerObjectsManager(), objectDisplayWeakRef);
                    Js::DynamicObject* object = debuggerObject->GetJSONObject(resolvedObject.scriptContext);
                    Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, object);
                    if (resolvedObjectDisplay->IsFake())
                    {
                        Js::JavascriptOperators::OP_SetElementI((Js::Var)debuggerOnlyPropertiesArray, Js::JavascriptNumber::ToVar(debuggerOnlyPropertiesArrayCount++, scriptContext), marshaledObj, scriptContext);
                    }
                    else
                    {
                        Js::JavascriptOperators::OP_SetElementI((Js::Var)propertiesArray, Js::JavascriptNumber::ToVar(propertiesArrayCount++, scriptContext), marshaledObj, scriptContext);
                    }
                    objectDisplayWeakRef->ReleaseStrongReference();
                    objectDisplayWeakRef.Detach();
                }
            }
        }

        walkerRef->ReleaseStrongReference();
    }

    JsrtDebugUtils::AddPropertyToObject(childrensObject, JsrtDebugPropertyId::propertiesCount, childrensCount, scriptContext);
    JsrtDebugUtils::AddPropertyToObject(childrensObject, JsrtDebugPropertyId::properties, propertiesArray, scriptContext);
    JsrtDebugUtils::AddPropertyToObject(childrensObject, JsrtDebugPropertyId::debuggerOnlyProperties, debuggerOnlyPropertiesArray, scriptContext);

    return childrensObject;
}

DebuggerObjectsManager::DebuggerObjectsManager(JsrtDebug* debugObject) :
    handleId(0),
    debugObject(debugObject),
    handleToDebuggerObjectsDictionary(nullptr),
    dataToDebuggerObjectsDictionary(nullptr)
{
    Assert(debugObject != nullptr);
}

DebuggerObjectsManager::~DebuggerObjectsManager()
{
    if (this->dataToDebuggerObjectsDictionary != nullptr)
    {
        AssertMsg(this->dataToDebuggerObjectsDictionary->Count() == 0, "Should have cleared all entries by now?");

        Adelete(this->GetDebugObjectArena(), this->dataToDebuggerObjectsDictionary);
        this->dataToDebuggerObjectsDictionary = nullptr;
    }

    if (this->handleToDebuggerObjectsDictionary != nullptr)
    {
        AssertMsg(this->handleToDebuggerObjectsDictionary->Count() == 0, "Should have cleared all entries by now?");

        Adelete(this->GetDebugObjectArena(), this->handleToDebuggerObjectsDictionary);
        this->handleToDebuggerObjectsDictionary = nullptr;
    }
}

void DebuggerObjectsManager::ClearAll()
{
    if (this->dataToDebuggerObjectsDictionary != nullptr)
    {
        this->dataToDebuggerObjectsDictionary->Clear();
    }

    if (this->handleToDebuggerObjectsDictionary != nullptr)
    {
        this->handleToDebuggerObjectsDictionary->Map([this](uint handle, DebuggerObjectBase* debuggerObject) {
            Adelete(this->GetDebugObjectArena(), debuggerObject);
        });
        this->handleToDebuggerObjectsDictionary->Clear();
    }

    this->handleId = 0;
}

ArenaAllocator * DebuggerObjectsManager::GetDebugObjectArena()
{
    return this->GetDebugObject()->GetDebugObjectArena();
}

bool DebuggerObjectsManager::TryGetDebuggerObjectFromHandle(uint handle, DebuggerObjectBase ** debuggerObject)
{
    if (this->handleToDebuggerObjectsDictionary == nullptr)
    {
        return false;
    }

    return this->handleToDebuggerObjectsDictionary->TryGetValue(handle, debuggerObject);
}

bool DebuggerObjectsManager::TryGetFrameObjectFromFrameIndex(uint frameIndex, DebuggerObjectBase ** debuggerObject)
{
    bool found = false;
    if (this->handleToDebuggerObjectsDictionary != nullptr)
    {
        this->handleToDebuggerObjectsDictionary->MapUntil([&](uint index, DebuggerObjectBase* debuggerObjectBase)
        {
            if (debuggerObjectBase != nullptr && debuggerObjectBase->GetType() == DebuggerObjectType_StackFrame)
            {
                DebuggerObjectStackFrame* stackFrame = (DebuggerObjectStackFrame*)debuggerObjectBase;
                if (stackFrame->GetIndex() == frameIndex)
                {
                    *debuggerObject = debuggerObjectBase;
                    found = true;
                    return true;
                }
            }
            return false;
        });
    }

    return found;
}

void DebuggerObjectsManager::AddToDebuggerObjectsDictionary(DebuggerObjectBase * debuggerObject)
{
    Assert(debuggerObject != nullptr);

    uint handle = debuggerObject->GetHandle();

    Assert(handle > 0);

    if (this->handleToDebuggerObjectsDictionary == nullptr)
    {
        this->handleToDebuggerObjectsDictionary = Anew(this->GetDebugObjectArena(), DebuggerObjectsDictionary, this->GetDebugObjectArena(), 10);
    }

    Assert(!this->handleToDebuggerObjectsDictionary->ContainsKey(handle));

    int index = this->handleToDebuggerObjectsDictionary->Add(handle, debuggerObject);

    Assert(index != -1);
}

void DebuggerObjectsManager::AddToDataToDebuggerObjectsDictionary(void * data, DebuggerObjectBase * debuggerObject)
{
    Assert(data != nullptr);
    Assert(debuggerObject != nullptr);

    if (this->dataToDebuggerObjectsDictionary == nullptr)
    {
        this->dataToDebuggerObjectsDictionary = Anew(this->GetDebugObjectArena(), DataToDebuggerObjectsDictionary, this->GetDebugObjectArena(), 10);
    }

    Assert(!this->dataToDebuggerObjectsDictionary->ContainsKey(data));

    int index = this->dataToDebuggerObjectsDictionary->Add(data, debuggerObject);

    Assert(index != -1);

    this->AddToDebuggerObjectsDictionary(debuggerObject);
}

bool DebuggerObjectsManager::TryGetDataFromDataToDebuggerObjectsDictionary(void * data, DebuggerObjectBase ** debuggerObject)
{
    if (this->dataToDebuggerObjectsDictionary == nullptr)
    {
        return false;
    }

    return this->dataToDebuggerObjectsDictionary->TryGetValue(data, debuggerObject);
}

DebuggerObjectStackFrame::DebuggerObjectStackFrame(DebuggerObjectsManager * debuggerObjectsManager, Js::DiagStackFrame * stackFrame, uint frameIndex) :
    DebuggerObjectBase(DebuggerObjectType::DebuggerObjectType_StackFrame, debuggerObjectsManager),
    frameIndex(frameIndex),
    stackFrame(stackFrame),
    pObjectModelWalker(nullptr),
    stackTraceObject(nullptr),
    propertiesObject(nullptr)
{
    Assert(this->stackFrame != nullptr);
}

DebuggerObjectStackFrame::~DebuggerObjectStackFrame()
{
    this->stackFrame = nullptr;

    if (this->pObjectModelWalker != nullptr)
    {
        HeapDelete(this->pObjectModelWalker);
        this->pObjectModelWalker = nullptr;
    }

    this->stackTraceObject = nullptr;
    this->propertiesObject = nullptr;
}

DebuggerObjectBase * DebuggerObjectStackFrame::Make(DebuggerObjectsManager * debuggerObjectsManager, Js::DiagStackFrame * stackFrame, uint frameIndex)
{
    DebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(stackFrame, &debuggerObject))
    {
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), DebuggerObjectStackFrame, debuggerObjectsManager, stackFrame, frameIndex);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(stackFrame, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * DebuggerObjectStackFrame::GetJSONObject(Js::ScriptContext* scriptContext)
{
    if (this->stackTraceObject != nullptr)
    {
        return this->stackTraceObject;
    }

    Js::ScriptContext *frameScriptContext = stackFrame->GetScriptContext();
    this->stackTraceObject = frameScriptContext->GetLibrary()->CreateObject();

    Js::FunctionBody* functionBody = stackFrame->GetFunction();
    Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();

    JsrtDebugUtils::AddPropertyToObject(this->stackTraceObject, JsrtDebugPropertyId::index, frameIndex, scriptContext);
    JsrtDebugUtils::AddScriptIdToObject(this->stackTraceObject, utf8SourceInfo);
    JsrtDebugUtils::AddFileNameToObject(this->stackTraceObject, utf8SourceInfo);

    int currentByteCodeOffset = stackFrame->GetByteCodeOffset();

    if (stackFrame->IsInterpreterFrame() && frameIndex != 0)
    {
        // For non-leaf interpreter frames back up 1 instruction so we see the caller
        // rather than the statement after the caller
        currentByteCodeOffset--;
    }

    JsrtDebugUtils::AddLineColumnToObject(this->stackTraceObject, functionBody, currentByteCodeOffset);
    JsrtDebugUtils::AddSourceLengthAndTextToObject(this->stackTraceObject, functionBody, currentByteCodeOffset);

    DebuggerObjectBase* functionObject = DebuggerObjectFunction::Make(this->GetDebuggerObjectsManager(), functionBody);
    JsrtDebugUtils::AddPropertyToObject(stackTraceObject, JsrtDebugPropertyId::functionHandle, functionObject->GetHandle(), frameScriptContext);
    
    DebuggerObjectBase* scriptObject = DebuggerObjectScript::Make(this->GetDebuggerObjectsManager(), utf8SourceInfo);
    JsrtDebugUtils::AddPropertyToObject(stackTraceObject, JsrtDebugPropertyId::scriptHandle, scriptObject->GetHandle(), frameScriptContext);

    JsrtDebugUtils::AddPropertyToObject(stackTraceObject, JsrtDebugPropertyId::handle, this->GetHandle(), frameScriptContext);

    return this->stackTraceObject;
}

Js::DynamicObject * DebuggerObjectStackFrame::GetLocalsObject()
{
    if (this->propertiesObject != nullptr)
    {
        return this->propertiesObject;
    }

    Js::ScriptContext* scriptContext = this->stackFrame->GetScriptContext();

    /*
        {
            "exception" : {},
            "arguments" : {},
            "returnValue" : {},
            "functionCallsReturn" : [{}, {}],
            "locals" : [],
            "scopes" : [{}, {}],
            "globals" : {}
        }
     */

    this->propertiesObject = scriptContext->GetLibrary()->CreateObject();

    Js::Var returnValueObject = nullptr;

    uint functionCallsReturnCount = 0;
    Js::JavascriptArray* functionCallsReturn = scriptContext->GetLibrary()->CreateArray();

    uint totalLocalsCount = 0;
    Js::JavascriptArray* localsArray = scriptContext->GetLibrary()->CreateArray();

    uint scopesCount = 0;
    Js::JavascriptArray* scopesArray = scriptContext->GetLibrary()->CreateArray();

    Js::DynamicObject* globalsObject = nullptr;

    if (this->pObjectModelWalker == nullptr)
    {
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();

        Js::IDiagObjectModelDisplay* pLocalsDisplay = Anew(pRefArena->Arena(), Js::LocalsDisplay, this->stackFrame);
        this->pObjectModelWalker = pLocalsDisplay->CreateWalker();
        Js::LocalsWalker* localsWalker = (Js::LocalsWalker*)this->pObjectModelWalker->GetStrongReference();

        ulong totalProperties = localsWalker->GetChildrenCount();
        if (totalProperties > 0)
        {
            int index = 0;
            Js::ResolvedObject resolvedObject;
            resolvedObject.scriptContext = this->stackFrame->GetScriptContext();

            DebuggerObjectsManager* debuggerObjectsManager = this->GetDebuggerObjectsManager();

            if (Js::VariableWalkerBase::GetExceptionObject(index, this->stackFrame, &resolvedObject))
            {
                DebuggerObjectBase::CreateDebuggerObject<DebuggerObjectProperty>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                {
                    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::exception, marshaledObj, scriptContext);
                });
            }

            if (localsWalker->HasUserNotDefinedArguments() && localsWalker->CreateArgumentsObject(&resolvedObject))
            {
                DebuggerObjectBase::CreateDebuggerObject<DebuggerObjectProperty>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                {
                    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::arguments, marshaledObj, scriptContext);
                });
            }

            Js::ReturnedValueList *returnedValueList = this->stackFrame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetReturnedValueList();

            if (returnedValueList != nullptr && returnedValueList->Count() > 0 && this->stackFrame->IsTopFrame())
            {
                for (int i = 0; i < returnedValueList->Count(); ++i)
                {
                    Js::ReturnedValue * returnValue = returnedValueList->Item(i);
                    Js::VariableWalkerBase::GetReturnedValueResolvedObject(returnValue, this->stackFrame, &resolvedObject);

                    DebuggerObjectBase::CreateDebuggerObject<DebuggerObjectProperty>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                    {

                        if (returnValue->isValueOfReturnStatement)
                        {
                            returnValueObject = marshaledObj;
                        }
                        else
                        {
                            Js::JavascriptOperators::OP_SetElementI((Js::Var)functionCallsReturn, Js::JavascriptNumber::ToVar(functionCallsReturnCount, scriptContext), marshaledObj, scriptContext);
                            functionCallsReturnCount++;
                        }
                    });
                }

                if (returnValueObject != nullptr)
                {
                    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::returnValue, returnValueObject, scriptContext);
                }

                if (functionCallsReturnCount > 0)
                {
                    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::functionCallsReturn, functionCallsReturn, scriptContext);
                }
            }

            ulong localsCount = localsWalker->GetLocalVariablesCount();
            for (ulong i = 0; i < localsCount; ++i)
            {
                if (!localsWalker->GetLocal(i, &resolvedObject))
                {
                    break;
                }

                DebuggerObjectBase::CreateDebuggerObject<DebuggerObjectProperty>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                {
                    Js::JavascriptOperators::OP_SetElementI((Js::Var)localsArray, Js::JavascriptNumber::ToVar(totalLocalsCount, scriptContext), marshaledObj, scriptContext);
                    totalLocalsCount++;
                });
            }


            index = 0;
            BOOL foundGroup = TRUE;
            while (foundGroup)
            {
                foundGroup = localsWalker->GetScopeObject(index++, &resolvedObject);
                if (foundGroup == TRUE)
                {
                    AutoPtr<WeakArenaReference<Js::IDiagObjectModelDisplay>> objectDisplayWeakRef = resolvedObject.GetObjectDisplay();
                    DebuggerObjectBase* debuggerObject = DebuggerObjectScope::Make(debuggerObjectsManager, objectDisplayWeakRef, scopesCount);
                    Js::DynamicObject* object = debuggerObject->GetJSONObject(resolvedObject.scriptContext);
                    Assert(object != nullptr);
                    Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, object);
                    Js::JavascriptOperators::OP_SetElementI((Js::Var)scopesArray, Js::JavascriptNumber::ToVar(scopesCount, scriptContext), marshaledObj, scriptContext);
                    scopesCount++;
                    objectDisplayWeakRef.Detach();
                }
            }

            if (localsWalker->GetGlobalsObject(&resolvedObject))
            {
                CreateDebuggerObject<DebuggerObjectGlobalsNode>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                {
                    globalsObject = (Js::DynamicObject*)marshaledObj;
                });
            }
        }

        this->pObjectModelWalker->ReleaseStrongReference();
        Adelete(pRefArena->Arena(), pLocalsDisplay);
    }

    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::locals, localsArray, scriptContext);
    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::scopes, scopesArray, scriptContext);

    if (globalsObject == nullptr)
    {
        globalsObject = scriptContext->GetLibrary()->CreateObject();
    }

    JsrtDebugUtils::AddPropertyToObject(this->propertiesObject, JsrtDebugPropertyId::globals, globalsObject, scriptContext);

    return this->propertiesObject;
}

Js::DynamicObject* DebuggerObjectStackFrame::Evaluate(const char16 * pszSrc, bool isLibraryCode)
{
    Js::DynamicObject* evalResult = nullptr;
    if (this->stackFrame != nullptr)
    {
        Js::ResolvedObject resolvedObject;
        HRESULT hr = S_OK;
        Js::ScriptContext* scriptContext = this->stackFrame->GetScriptContext();
        Js::JavascriptExceptionObject *exceptionObject = nullptr;
        {
            BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(scriptContext, false)
            {
                this->stackFrame->EvaluateImmediate(pszSrc, isLibraryCode, &resolvedObject);
            }
            END_JS_RUNTIME_CALL_AND_TRANSLATE_AND_GET_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr, scriptContext, exceptionObject);
        }
        if (resolvedObject.obj == nullptr)
        {
            resolvedObject.name = _u("{exception}");
            resolvedObject.typeId = Js::TypeIds_Error;
            resolvedObject.address = nullptr;

            if (exceptionObject != nullptr)
            {
                resolvedObject.obj = exceptionObject->GetThrownObject(scriptContext);
            }
            else
            {
                resolvedObject.obj = scriptContext->GetLibrary()->GetUndefined();
            }
        }
        if (resolvedObject.obj != nullptr)
        {
            resolvedObject.scriptContext = scriptContext;

            charcount_t len = Js::JavascriptString::GetBufferLength(pszSrc);
            resolvedObject.name = AnewNoThrowArray(this->GetDebuggerObjectsManager()->GetDebugObjectArena(), WCHAR, len + 1);
            if (resolvedObject.name == nullptr)
            {
                return nullptr;
            }
            wcscpy_s((WCHAR*)resolvedObject.name, len + 1, pszSrc);

            resolvedObject.typeId = Js::JavascriptOperators::GetTypeId(resolvedObject.obj);
            DebuggerObjectBase::CreateDebuggerObject<DebuggerObjectProperty>(this->GetDebuggerObjectsManager(), resolvedObject, this->stackFrame->GetScriptContext(), [&](Js::Var marshaledObj)
            {
                evalResult = (Js::DynamicObject*)marshaledObj;
            });
        }
    }
    return evalResult;
}

DebuggerObjectScript::DebuggerObjectScript(DebuggerObjectsManager * debuggerObjectsManager, Js::Utf8SourceInfo * utf8SourceInfo) :
    DebuggerObjectBase(DebuggerObjectType::DebuggerObjectType_Script, debuggerObjectsManager),
    utf8SourceInfo(utf8SourceInfo),
    sourceObject(nullptr)
{
    Assert(utf8SourceInfo != nullptr);
}

DebuggerObjectScript::~DebuggerObjectScript()
{
    this->utf8SourceInfo = nullptr;
    this->sourceObject = nullptr;
}

DebuggerObjectBase * DebuggerObjectScript::Make(DebuggerObjectsManager * debuggerObjectsManager, Js::Utf8SourceInfo * utf8SourceInfo)
{
    DebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(utf8SourceInfo, &debuggerObject))
    {
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), DebuggerObjectScript, debuggerObjectsManager, utf8SourceInfo);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(utf8SourceInfo, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * DebuggerObjectScript::GetJSONObject(Js::ScriptContext* scriptContext)
{
    if (this->sourceObject != nullptr)
    {
        return this->sourceObject;
    }

    Js::ScriptContext* utf8SourceScriptContext = this->utf8SourceInfo->GetScriptContext();

    this->sourceObject = utf8SourceScriptContext->GetLibrary()->CreateObject();

    JsrtDebugUtils::AddScriptIdToObject(this->sourceObject, utf8SourceInfo);
    JsrtDebugUtils::AddFileNameToObject(this->sourceObject, utf8SourceInfo);
    JsrtDebugUtils::AddLineCountToObject(this->sourceObject, utf8SourceInfo);
    JsrtDebugUtils::AddPropertyToObject(this->sourceObject, JsrtDebugPropertyId::sourceLength, utf8SourceInfo->GetCchLength(), utf8SourceInfo->GetScriptContext());
    JsrtDebugUtils::AddPropertyToObject(this->sourceObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);

    return this->sourceObject;
}

DebuggerObjectProperty::DebuggerObjectProperty(DebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay) :
    DebuggerObjectBase(DebuggerObjectType::DebuggerObjectType_Property, debuggerObjectsManager),
    objectDisplay(objectDisplay),
    propertyObject(nullptr),
    walkerRef(nullptr)
{
    Assert(objectDisplay != nullptr);
}

DebuggerObjectProperty::~DebuggerObjectProperty()
{
    if (this->objectDisplay != nullptr)
    {
        HeapDelete(this->objectDisplay);
        this->objectDisplay = nullptr;
    }

    if (this->walkerRef != nullptr)
    {
        HeapDelete(this->walkerRef);
        this->walkerRef = nullptr;
    }

    this->propertyObject = nullptr;
}

DebuggerObjectBase * DebuggerObjectProperty::Make(DebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay)
{
    DebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(objectDisplay, &debuggerObject))
    {
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), DebuggerObjectProperty, debuggerObjectsManager, objectDisplay);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(objectDisplay, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * DebuggerObjectProperty::GetJSONObject(Js::ScriptContext* scriptContext)
{
    if (this->propertyObject != nullptr)
    {
        return this->propertyObject;
    }

    Js::IDiagObjectModelDisplay* objectDisplayRef = this->objectDisplay->GetStrongReference();
    if (objectDisplayRef != nullptr)
    {
        this->propertyObject = scriptContext->GetLibrary()->CreateObject();

        JsrtDebugUtils::AddPropertyToObject(this->propertyObject, JsrtDebugPropertyId::name, objectDisplayRef->Name(), scriptContext);

        JsrtDebugUtils::AddPropertyType(this->propertyObject, objectDisplayRef, scriptContext); // Will add type, value, display, className, propertyAttributes

        JsrtDebugUtils::AddPropertyToObject(this->propertyObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);

        this->objectDisplay->ReleaseStrongReference();
    }

    return this->propertyObject;
}

Js::DynamicObject* DebuggerObjectProperty::GetChildrens(Js::ScriptContext* scriptContext, uint fromCount, uint totalCount)
{
    Js::IDiagObjectModelDisplay* objectDisplayRef = objectDisplay->GetStrongReference();
    if (objectDisplayRef == nullptr)
    {
        return nullptr;
    }

    if (this->walkerRef == nullptr)
    {
        this->walkerRef = objectDisplayRef->CreateWalker();
    }

    Js::DynamicObject* childrens = __super::GetChildrens(this->walkerRef, scriptContext, fromCount, totalCount);

    objectDisplay->ReleaseStrongReference();

    return childrens;
}

DebuggerObjectScope::DebuggerObjectScope(DebuggerObjectsManager * debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay, uint index) :
    DebuggerObjectBase(DebuggerObjectType::DebuggerObjectType_Scope, debuggerObjectsManager),
    objectDisplay(objectDisplay),
    index(index),
    scopeObject(nullptr),
    walkerRef(nullptr)
{
    Assert(this->objectDisplay != nullptr);
}

DebuggerObjectScope::~DebuggerObjectScope()
{
    if (this->objectDisplay != nullptr)
    {
        HeapDelete(this->objectDisplay);
        this->objectDisplay = nullptr;
    }

    if (this->walkerRef != nullptr)
    {
        HeapDelete(this->walkerRef);
        this->walkerRef = nullptr;
    }

    this->scopeObject = nullptr;
}

DebuggerObjectBase * DebuggerObjectScope::Make(DebuggerObjectsManager * debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay, uint index)
{
    DebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(objectDisplay, &debuggerObject))
    {
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), DebuggerObjectScope, debuggerObjectsManager, objectDisplay, index);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(objectDisplay, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * DebuggerObjectScope::GetJSONObject(Js::ScriptContext* scriptContext)
{
    if (this->scopeObject != nullptr)
    {
        return this->scopeObject;
    }

    Js::IDiagObjectModelDisplay* modelDisplay = this->objectDisplay->GetStrongReference();
    if (modelDisplay != nullptr)
    {
        this->scopeObject = scriptContext->GetLibrary()->CreateObject();
        JsrtDebugUtils::AddPropertyToObject(this->scopeObject, JsrtDebugPropertyId::index, this->index, scriptContext);
        JsrtDebugUtils::AddPropertyToObject(this->scopeObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);

        this->objectDisplay->ReleaseStrongReference();
    }

    return this->scopeObject;
}

Js::DynamicObject * DebuggerObjectScope::GetChildrens(Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{
    Js::IDiagObjectModelDisplay* objectDisplayRef = objectDisplay->GetStrongReference();
    if (objectDisplayRef == nullptr)
    {
        return nullptr;
    }

    if (this->walkerRef == nullptr)
    {
        this->walkerRef = objectDisplayRef->CreateWalker();
    }

    Js::DynamicObject* childrens = __super::GetChildrens(this->walkerRef, scriptContext, fromCount, totalCount);

    objectDisplay->ReleaseStrongReference();

    return childrens;
}

DebuggerObjectFunction::DebuggerObjectFunction(DebuggerObjectsManager* debuggerObjectsManager, Js::FunctionBody* functionBody) :
    DebuggerObjectBase(DebuggerObjectType::DebuggerObjectType_Function, debuggerObjectsManager),
    functionBody(functionBody),
    functionObject(nullptr)
{
}

DebuggerObjectFunction::~DebuggerObjectFunction()
{
    this->functionBody = nullptr;
    this->functionObject = nullptr;
}

DebuggerObjectBase * DebuggerObjectFunction::Make(DebuggerObjectsManager * debuggerObjectsManager, Js::FunctionBody * functionBody)
{
    DebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(functionBody, &debuggerObject))
    {
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), DebuggerObjectFunction, debuggerObjectsManager, functionBody);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(functionBody, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * DebuggerObjectFunction::GetJSONObject(Js::ScriptContext * scriptContext)
{
    if (this->functionObject != nullptr)
    {
        return this->functionObject;
    }

    this->functionObject = scriptContext->GetLibrary()->CreateObject();
    if (this->functionBody != nullptr)
    {
        JsrtDebugUtils::AddScriptIdToObject(this->functionObject, this->functionBody->GetUtf8SourceInfo());
        JsrtDebugUtils::AddPropertyToObject(this->functionObject, JsrtDebugPropertyId::line, this->functionBody->GetLineNumber(), scriptContext);
        JsrtDebugUtils::AddPropertyToObject(this->functionObject, JsrtDebugPropertyId::column, this->functionBody->GetColumnNumber(), scriptContext);
        JsrtDebugUtils::AddPropertyToObject(this->functionObject, JsrtDebugPropertyId::name, this->functionBody->GetDisplayName(), scriptContext);
        JsrtDebugUtils::AddPropertyToObject(this->functionObject, JsrtDebugPropertyId::type, scriptContext->GetLibrary()->GetFunctionTypeDisplayString()->GetSz(), scriptContext);
        JsrtDebugUtils::AddPropertyToObject(this->functionObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);
    }
    return this->functionObject;
}

DebuggerObjectGlobalsNode::DebuggerObjectGlobalsNode(DebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay) :
    DebuggerObjectBase(DebuggerObjectType::DebuggerObjectType_Globals, debuggerObjectsManager),
    objectDisplay(objectDisplay),
    propertyObject(nullptr),
    walkerRef(nullptr)
{
    Assert(objectDisplay != nullptr);
}

DebuggerObjectGlobalsNode::~DebuggerObjectGlobalsNode()
{
    if (this->objectDisplay != nullptr)
    {
        HeapDelete(this->objectDisplay);
        this->objectDisplay = nullptr;
    }

    if (this->walkerRef != nullptr)
    {
        HeapDelete(this->walkerRef);
        this->walkerRef = nullptr;
    }

    this->propertyObject = nullptr;
}

DebuggerObjectBase * DebuggerObjectGlobalsNode::Make(DebuggerObjectsManager * debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay)
{
    DebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(objectDisplay, &debuggerObject))
    {
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), DebuggerObjectGlobalsNode, debuggerObjectsManager, objectDisplay);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(objectDisplay, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * DebuggerObjectGlobalsNode::GetJSONObject(Js::ScriptContext * scriptContext)
{
    if (this->propertyObject != nullptr)
    {
        return this->propertyObject;
    }

    Js::IDiagObjectModelDisplay* objectDisplayRef = this->objectDisplay->GetStrongReference();
    if (objectDisplayRef != nullptr)
    {
        this->propertyObject = scriptContext->GetLibrary()->CreateObject();
        JsrtDebugUtils::AddPropertyToObject(this->propertyObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);
        this->objectDisplay->ReleaseStrongReference();
    }

    return this->propertyObject;
}

Js::DynamicObject * DebuggerObjectGlobalsNode::GetChildrens(Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{
    Js::IDiagObjectModelDisplay* objectDisplayRef = objectDisplay->GetStrongReference();
    if (objectDisplayRef == nullptr)
    {
        return nullptr;
    }

    if (this->walkerRef == nullptr)
    {
        this->walkerRef = objectDisplayRef->CreateWalker();
    }

    Js::DynamicObject* childrens = __super::GetChildrens(this->walkerRef, scriptContext, fromCount, totalCount);

    objectDisplay->ReleaseStrongReference();

    return childrens;
}

JsrtDebugStackFrames::JsrtDebugStackFrames(JsrtDebug* debugObject):
    stackTraceArray(nullptr),
    framesDictionary(nullptr)
{
    Assert(debugObject != nullptr);
    this->debugObject = debugObject;
}

JsrtDebugStackFrames::~JsrtDebugStackFrames()
{
    this->stackTraceArray = nullptr;

    if (this->framesDictionary != nullptr)
    {
        // Just clear the dictionary, DebuggerObjectStackFrame will be delete when we call DebuggerObjectsManager::ClearAll 
        this->framesDictionary->Clear();
        this->framesDictionary = nullptr;
    }
}

Js::JavascriptArray * JsrtDebugStackFrames::StackFrames(Js::ScriptContext * scriptContext)
{
    if (this->stackTraceArray != nullptr)
    {
        return this->stackTraceArray;
    }

    this->stackTraceArray = scriptContext->GetLibrary()->CreateArray();

    Assert(this->framesDictionary == nullptr);

    this->framesDictionary = Anew(this->debugObject->GetDebugObjectArena(), FramesDictionary, this->debugObject->GetDebugObjectArena(), 10);

    uint frameCount = 0;

    for (Js::ScriptContext *tempScriptContext = scriptContext->GetThreadContext()->GetScriptContextList();
    tempScriptContext != nullptr && tempScriptContext->IsScriptContextInDebugMode();
        tempScriptContext = tempScriptContext->next)
    {
        Js::WeakDiagStack * framePointers = tempScriptContext->GetDebugContext()->GetProbeContainer()->GetFramePointers();
        if (framePointers != nullptr)
        {
            Js::DiagStack* stackFrames = framePointers->GetStrongReference();
            if (stackFrames != nullptr)
            {
                int count = stackFrames->Count();
                for (int frameIndex = 0; frameIndex < count; ++frameIndex)
                {
                    Js::DiagStackFrame* stackFrame = stackFrames->Peek(frameIndex);
                    Js::DynamicObject* stackTraceObject = this->GetStackFrame(stackFrame, frameCount);

                    Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, stackTraceObject);
                    Js::JavascriptOperators::OP_SetElementI((Js::Var)stackTraceArray, Js::JavascriptNumber::ToVar(frameCount++, scriptContext), marshaledObj, scriptContext);
                }
            }
            framePointers->ReleaseStrongReference();
            HeapDelete(framePointers);
        }
    }

    return this->stackTraceArray;
}

bool JsrtDebugStackFrames::TryGetFrameObjectFromFrameIndex(uint frameIndex, DebuggerObjectBase ** debuggerObject)
{
    if (this->framesDictionary != nullptr)
    {
        return this->framesDictionary->TryGetValue(frameIndex, debuggerObject);
    }

    return false;
}

Js::DynamicObject * JsrtDebugStackFrames::GetStackFrame(Js::DiagStackFrame * stackFrame, uint frameIndex)
{
    DebuggerObjectBase* debuggerObject = DebuggerObjectStackFrame::Make(this->debugObject->GetDebuggerObjectsManager(), stackFrame, frameIndex);

    Assert(this->framesDictionary != nullptr);

    this->framesDictionary->Add(frameIndex, debuggerObject);

    return debuggerObject->GetJSONObject(stackFrame->GetScriptContext());
}
