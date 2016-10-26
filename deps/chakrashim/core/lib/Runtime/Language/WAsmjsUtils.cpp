//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(ASMJS_PLAT) || defined(ENABLE_WASM)

namespace WAsmJs 
{
#if DBG_DUMP
    void RegisterSpace::PrintTmpRegisterAllocation(RegSlot loc)
    {
        if (PHASE_ON1(Js::AsmjsTmpRegisterAllocationPhase))
        {
            switch (mType)
            {
            case NONE   : break;
            case INT32  : Output::Print(_u("+I32 %d\n"), loc); break;
            case INT64  : Output::Print(_u("+I64 %d\n"), loc);break;
            case FLOAT32: Output::Print(_u("+F32 %d\n"), loc);break;
            case FLOAT64: Output::Print(_u("+F64 %d\n"), loc);break;
            case SIMD   : Output::Print(_u("+SIMD %d\n"), loc);break;
            default     : break;
            }
        }
    }

    void RegisterSpace::PrintTmpRegisterDeAllocation(RegSlot loc)
    {
        if (PHASE_ON1(Js::AsmjsTmpRegisterAllocationPhase))
        {
            switch (mType)
            {
            case NONE   : break;
            case INT32  : Output::Print(_u("-I32 %d\n"), loc); break;
            case INT64  : Output::Print(_u("-I64 %d\n"), loc);break;
            case FLOAT32: Output::Print(_u("-F32 %d\n"), loc);break;
            case FLOAT64: Output::Print(_u("-F64 %d\n"), loc);break;
            case SIMD   : Output::Print(_u("-SIMD %d\n"), loc);break;
            default     : break;
            }
        }
    }
#endif
};

#endif
