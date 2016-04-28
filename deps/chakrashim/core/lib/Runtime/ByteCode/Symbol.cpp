//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

#if DBG_DUMP
static const char16 * const SymbolTypeNames[] = { _u("Function"), _u("Variable"), _u("MemberName"), _u("Formal"), _u("Unknown") };
#endif

bool Symbol::GetIsArguments() const
{
    return decl != nullptr && (decl->grfpn & PNodeFlags::fpnArguments);
}

Js::PropertyId Symbol::EnsurePosition(ByteCodeGenerator* byteCodeGenerator)
{
    // Guarantee that a symbol's name has a property ID mapping.
    if (this->position == Js::Constants::NoProperty)
    {
        this->position = this->EnsurePositionNoCheck(byteCodeGenerator->TopFuncInfo());
    }
    return this->position;
}

Js::PropertyId Symbol::EnsurePosition(FuncInfo *funcInfo)
{
    // Guarantee that a symbol's name has a property ID mapping.
    if (this->position == Js::Constants::NoProperty)
    {
        this->position = this->EnsurePositionNoCheck(funcInfo);
    }
    return this->position;
}

Js::PropertyId Symbol::EnsurePositionNoCheck(FuncInfo *funcInfo)
{
    return funcInfo->byteCodeFunction->GetOrAddPropertyIdTracked(this->GetName());
}

void Symbol::SaveToPropIdArray(Symbol *sym, Js::PropertyIdArray *propIds, ByteCodeGenerator *byteCodeGenerator, Js::PropertyId *pFirstSlot /* = null */)
{
    if (sym)
    {
        Js::PropertyId slot = sym->scopeSlot;
        if (slot != Js::Constants::NoProperty)
        {
            Assert((uint32)slot < propIds->count);
            propIds->elements[slot] = sym->EnsurePosition(byteCodeGenerator);
            if (pFirstSlot && !sym->GetIsArguments())
            {
                if (*pFirstSlot == Js::Constants::NoProperty ||
                    *pFirstSlot > slot)
                {
                    *pFirstSlot = slot;
                }
            }
        }
    }
}

bool Symbol::NeedsSlotAlloc(FuncInfo *funcInfo)
{
    return IsInSlot(funcInfo, true);
}

bool Symbol::IsInSlot(FuncInfo *funcInfo, bool ensureSlotAlloc)
{
    if (this->GetIsGlobal())
    {
        return false;
    }
    if (funcInfo->GetHasHeapArguments() && this->GetIsFormal() && ByteCodeGenerator::NeedScopeObjectForArguments(funcInfo, funcInfo->root))
    {
        return true;
    }
    if (this->GetIsGlobalCatch())
    {
        return true;
    }
    if (this->scope->GetCapturesAll())
    {
        return true;
    }
    // If body and param scopes are not merged then an inner scope slot is used
    if (!this->GetIsArguments() && this->scope->GetScopeType() == ScopeType_Parameter && !this->scope->GetCanMergeWithBodyScope())
    {
        return true;
    }

    return this->GetHasNonLocalReference() && (ensureSlotAlloc || this->GetIsCommittedToSlot());
}

bool Symbol::GetIsCommittedToSlot() const
{
    return isCommittedToSlot || this->scope->GetFunc()->GetCallsEval() || this->scope->GetFunc()->GetChildCallsEval();
}

Js::PropertyId Symbol::EnsureScopeSlot(FuncInfo *funcInfo)
{
    if (this->NeedsSlotAlloc(funcInfo) && this->scopeSlot == Js::Constants::NoProperty)
    {
        this->scopeSlot = this->scope->AddScopeSlot();
    }
    return this->scopeSlot;
}

void Symbol::SetHasNonLocalReference(bool b, ByteCodeGenerator *byteCodeGenerator)
{
    this->hasNonLocalReference = b;

    // The symbol's home function will tell us which child function we're currently processing.
    // This is the one that captures the symbol, from the declaring function's perspective.
    // So based on that information, note either that, (a.) the symbol is committed to the heap from its
    // inception, (b.) the symbol must be committed when the capturing function is instantiated.

    FuncInfo *funcHome = this->scope->GetFunc();
    FuncInfo *funcChild = funcHome->GetCurrentChildFunction();

    // If this is not a local property, or not all its references can be tracked, or
    // it's not scoped to the function, or we're in debug mode, disable the delayed capture optimization.
    if (funcHome->IsGlobalFunction() ||
        funcHome->GetCallsEval() ||
        funcHome->GetChildCallsEval() ||
        funcChild == nullptr ||
        this->GetScope() != funcHome->GetBodyScope() ||
        byteCodeGenerator->IsInDebugMode() ||
        PHASE_OFF(Js::DelayCapturePhase, funcHome->byteCodeFunction))
    {
        this->SetIsCommittedToSlot();
    }

    if (this->isCommittedToSlot)
    {
        return;
    }

    AnalysisAssert(funcChild);
    ParseNode *pnodeChild = funcChild->root;

    Assert(pnodeChild && pnodeChild->nop == knopFncDecl);

    if (pnodeChild->sxFnc.IsDeclaration())
    {
        // The capturing function is a declaration but may still be limited to an inner scope.
        Scope *scopeChild = funcHome->GetCurrentChildScope();
        if (scopeChild == this->scope || scopeChild->GetScopeType() == ScopeType_FunctionBody)
        {
            // The symbol is captured on entry to the scope in which it's declared.
            // (Check the scope type separately so that we get the special parameter list and
            // named function expression cases as well.)
            this->SetIsCommittedToSlot();
            return;
        }
    }

    // There is a chance we can limit the region in which the symbol lives on the heap.
    // Note which function captures the symbol.
    funcChild->AddCapturedSym(this);
}

void Symbol::SetHasMaybeEscapedUse(ByteCodeGenerator * byteCodeGenerator)
{
    Assert(!this->GetIsMember());
    if (!hasMaybeEscapedUse)
    {
        SetHasMaybeEscapedUseInternal(byteCodeGenerator);
    }
}

void Symbol::SetHasMaybeEscapedUseInternal(ByteCodeGenerator * byteCodeGenerator)
{
    Assert(!hasMaybeEscapedUse);
    Assert(!this->GetIsFormal());
    hasMaybeEscapedUse = true;
    if (PHASE_TESTTRACE(Js::StackFuncPhase, byteCodeGenerator->TopFuncInfo()->byteCodeFunction))
    {
        Output::Print(_u("HasMaybeEscapedUse: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }
    if (this->GetHasFuncAssignment())
    {
        this->GetScope()->GetFunc()->SetHasMaybeEscapedNestedFunc(
            DebugOnly(this->symbolType == STFunction ? _u("MaybeEscapedUseFuncDecl") : _u("MaybeEscapedUse")));
    }
}

void Symbol::SetHasFuncAssignment(ByteCodeGenerator * byteCodeGenerator)
{
    Assert(!this->GetIsMember());
    if (!hasFuncAssignment)
    {
        SetHasFuncAssignmentInternal(byteCodeGenerator);
    }
}

void Symbol::SetHasFuncAssignmentInternal(ByteCodeGenerator * byteCodeGenerator)
{
    Assert(!hasFuncAssignment);
    hasFuncAssignment = true;
    FuncInfo * top = byteCodeGenerator->TopFuncInfo();
    if (PHASE_TESTTRACE(Js::StackFuncPhase, top->byteCodeFunction))
    {
        Output::Print(_u("HasFuncAssignment: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }

    if (this->GetHasMaybeEscapedUse() || this->GetScope()->GetIsObject())
    {
        byteCodeGenerator->TopFuncInfo()->SetHasMaybeEscapedNestedFunc(DebugOnly(
            this->GetIsFormal() ? _u("FormalAssignment") :
            this->GetScope()->GetIsObject() ? _u("ObjectScopeAssignment") :
            _u("MaybeEscapedUse")));
    }
}

void Symbol::RestoreHasFuncAssignment()
{
    Assert(hasFuncAssignment == (this->symbolType == STFunction));
    Assert(this->GetIsFormal() || !this->GetHasMaybeEscapedUse());
    hasFuncAssignment = true;
    if (PHASE_TESTTRACE1(Js::StackFuncPhase))
    {
        Output::Print(_u("RestoreHasFuncAssignment: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }
}

Symbol * Symbol::GetFuncScopeVarSym() const
{
    if (!this->GetIsBlockVar())
    {
        return nullptr;
    }
    FuncInfo * parentFuncInfo = this->GetScope()->GetFunc();
    if (parentFuncInfo->GetIsStrictMode())
    {
        return nullptr;
    }
    Symbol *fncScopeSym = parentFuncInfo->GetBodyScope()->FindLocalSymbol(this->GetName());
    if (fncScopeSym == nullptr && parentFuncInfo->GetParamScope() != nullptr)
    {
        // We couldn't find the sym in the body scope, try finding it in the parameter scope.
        Scope* paramScope = parentFuncInfo->GetParamScope();
        fncScopeSym = paramScope->FindLocalSymbol(this->GetName());
        if (fncScopeSym == nullptr)
        {
            FuncInfo* parentParentFuncInfo = paramScope->GetEnclosingScope()->GetFunc();
            if (parentParentFuncInfo->root->sxFnc.IsAsync())
            {
                // In the case of async functions the func-scope var sym might have
                // come from the parent function parameter scope due to the syntax
                // desugaring implementation of async functions.
                fncScopeSym = parentParentFuncInfo->GetBodyScope()->FindLocalSymbol(this->GetName());
                if (fncScopeSym == nullptr)
                {
                    fncScopeSym = parentParentFuncInfo->GetParamScope()->FindLocalSymbol(this->GetName());
                }
            }
        }
    }
    Assert(fncScopeSym);
    // Parser should have added a fake var decl node for block scoped functions in non-strict mode
    // IsBlockVar() indicates a user let declared variable at function scope which
    // shadows the function's var binding, thus only emit the var binding init if
    // we do not have a block var symbol.
    if (!fncScopeSym || fncScopeSym->GetIsBlockVar())
    {
        return nullptr;
    }
    return fncScopeSym;
}

#if DBG_DUMP
const char16 * Symbol::GetSymbolTypeName()
{
    return SymbolTypeNames[symbolType];
}
#endif
