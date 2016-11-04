//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "WasmReaderPch.h"

#ifdef ENABLE_WASM

namespace Wasm
{
    WasmElementSegment::WasmElementSegment(ArenaAllocator * alloc, const UINT32 index, const WasmNode initExpr, const UINT32 numElem) :
        m_alloc(alloc),
        m_index(index),
        m_offsetExpr(initExpr),
        m_numElem(numElem),
        m_offset(0),
        m_limit(0),
        m_elemIdx(0),
        m_isOffsetResolved(false),
        m_elems(nullptr)
    {}

    void
    WasmElementSegment::Init(const Js::WebAssemblyModule& module)
    {
        Assert(m_numElem > 0);
        m_elems = AnewArray(m_alloc, UINT32, m_numElem);
        memset(m_elems, Js::Constants::UninitializedValue, m_numElem * sizeof(UINT32));
    }

    void WasmElementSegment::ResolveOffsets(const Js::WebAssemblyModule& module)
    {
        if (m_elems == nullptr)
        {
            return;
        }
        m_offset = module.GetOffsetFromInit(m_offsetExpr); // i32 or global (i32)
        m_limit = UInt32Math::Add(m_offset, m_numElem);

        if (m_limit > module.GetTableSize())
        {
            throw WasmCompilationException(_u("Out of bounds element in Table[%d][%d], max index: %d"), m_index, m_limit - 1, module.GetTableSize() - 1);
        }
        m_isOffsetResolved = true;
    }

    void
    WasmElementSegment::AddElement(const UINT32 funcIndex, const Js::WebAssemblyModule& module)
    {
        if (m_elems == nullptr)
        {
            Init(module);
        }
        Assert(m_elemIdx < m_numElem);
        m_elems[m_elemIdx++] = funcIndex;
    }

    inline bool 
    WasmElementSegment::IsOffsetResolved() const
    {
        return m_isOffsetResolved;
    }

    UINT32
    WasmElementSegment::GetElement(const UINT32 tableIndex) const
    {
        if (!IsOffsetResolved())
        {
            throw WasmCompilationException(_u("Offset not resolved: Cannot access table elements."));
        }
        if (m_offset > tableIndex || tableIndex >= m_limit)
        {
            return Js::Constants::UninitializedValue;
        }
        return m_elems[tableIndex - m_offset];
    }

} // namespace Wasm

#endif // ENABLE_WASM
