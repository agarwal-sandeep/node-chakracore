//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

const blob = WScript.LoadBinaryFile('dropteelocal.wasm')
const view = new Uint8Array(blob);
var a = Wasm.instantiateModule(view, {}).exports;
print(a.tee(1)); // == 100
