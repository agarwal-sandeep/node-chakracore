//---------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
//----------------------------------------------------------------------------

#pragma once

BEGIN_ENUM_UINT(JsrtDebugPropertyId)
#define DEBUGOBJECTPROPERTY(n) n,
#include "JsrtDebugPropertiesEnum.h"
END_ENUM_UINT()

enum JsrtDebugPropertyAttribute
{
    PROPERTY_ATTRIBUTE_NONE = 0x0,
    PROPERTY_ATTRIBUTE_HAVE_CHILDRENS = 0x1,
    PROPERTY_ATTRIBUTE_READ_ONLY_VALUE = 0x2
};

class JsrtDebugUtils
{
public:
    static void AddScriptIdToObject(Js::DynamicObject* object, Js::Utf8SourceInfo* utf8SourceInfo);
    static void AddFileNameOrScriptTypeToObject(Js::DynamicObject* object, Js::Utf8SourceInfo* utf8SourceInfo);
    static void AddLineColumnToObject(Js::DynamicObject* object, Js::FunctionBody* functionBody, int byteCodeOffset);
    static void AddSourceLengthAndTextToObject(Js::DynamicObject* object, Js::FunctionBody* functionBody, int byteCodeOffset);
    static void AddLineCountToObject(Js::DynamicObject* object, Js::Utf8SourceInfo* utf8SourceInfo);
    static void AddSouceToObject(Js::DynamicObject* object, Js::Utf8SourceInfo* utf8SourceInfo);

    static void AddVarPropertyToObject(Js::DynamicObject* object, const char16* propertyName, Js::Var value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, double value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, UINT value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, ULONG value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, LONG value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, const char16 * value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, bool value, Js::ScriptContext* scriptContext);
    static void AddPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, Js::Var value, Js::ScriptContext* scriptContext);

    static void AddPropertyType(Js::DynamicObject * object, Js::IDiagObjectModelDisplay* objectDisplayRef, Js::ScriptContext * scriptContext);

private:
    static void AddVarPropertyToObject(Js::DynamicObject* object, JsrtDebugPropertyId propertyId, Js::Var value, Js::ScriptContext* scriptContext);
    static char16* GetClassName(Js::TypeId typeId);
    static char16* GetDebugPropertyName(JsrtDebugPropertyId propertyId);
};
