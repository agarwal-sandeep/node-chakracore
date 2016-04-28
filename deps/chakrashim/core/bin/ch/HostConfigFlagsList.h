//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifdef FLAG
FLAG(BSTR, dbgbaseline,                     "Baseline file to compare debugger output", NULL)
FLAG(BSTR, DebugLaunch,                     "Create the test debugger and execute test in the debug mode", NULL)
FLAG(BSTR, GenerateLibraryByteCodeHeader,   "Generate bytecode header file from library code", NULL)
FLAG(int,  InspectMaxStringLength,          "Max string length to dump in locals inspection", 16)
FLAG(BSTR, Serialized,                      "If source is UTF8, deserializes from bytecode file", NULL)
#undef FLAG
#endif
