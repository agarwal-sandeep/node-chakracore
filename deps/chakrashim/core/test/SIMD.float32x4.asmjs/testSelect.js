//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.WScript.LoadScriptFile("..\\UnitTestFramework\\SimdJsHelpers.js");
function asmModule(stdlib, imports) {
    "use asm";
    var b4  = stdlib.SIMD.Bool32x4;
    var b8  = stdlib.SIMD.Bool16x8;
    var b16 = stdlib.SIMD.Bool8x16;
    
    var b4check  = b4.check;
    var b8check  = b8.check;
    var b16check = b16.check;
    
    var b4and  = b4.and;
    var b8and  = b8.and;
    var b16and = b16.and;
    
    var b4or = b4.or;
    var b8or = b8.or;
    var b16or= b16.or;
    
    var b4xor = b4.xor;
    var b8xor = b8.xor;
    var b16xor= b16.xor;
    
    var b4not = b4.not;
    var b8not  = b8.not;
    var b16not = b16.not;
    
    var b4allTrue = b4.allTrue;
    var b8allTrue  = b8.allTrue;
    var b16allTrue = b16.allTrue;
    
    var b4anyTrue = b4.anyTrue;
    var b8anyTrue  = b8.anyTrue;
    var b16anyTrue = b16.anyTrue;
    
    var i4 = stdlib.SIMD.Int32x4;
    var i4check = i4.check;
    var i4splat = i4.splat;
    var i4fromFloat32x4 = i4.fromFloat32x4;
    var i4fromFloat32x4Bits = i4.fromFloat32x4Bits;

    var i4neg = i4.neg;
    var i4add = i4.add;
    var i4sub = i4.sub;
    var i4mul = i4.mul;
    var i4lessThan = i4.lessThan;
    var i4equal = i4.equal;
    var i4greaterThan = i4.greaterThan;
    var i4select = i4.select;
    var i4and = i4.and;
    var i4or = i4.or;
    var i4xor = i4.xor;
    var i4not = i4.not;


    var f4 = stdlib.SIMD.Float32x4;  
    var f4check = f4.check;
    var f4splat = f4.splat;
    var f4fromInt32x4 = f4.fromInt32x4;
    var f4fromInt32x4Bits = f4.fromInt32x4Bits;
    var f4abs = f4.abs;
    var f4neg = f4.neg;
    var f4add = f4.add;
    var f4sub = f4.sub;
    var f4mul = f4.mul;
    var f4div = f4.div;
    
    var f4min = f4.min;
    var f4max = f4.max;
    
    var f4sqrt = f4.sqrt;
    var f4lessThan = f4.lessThan;
    var f4lessThanOrEqual = f4.lessThanOrEqual;
    var f4equal = f4.equal;
    var f4notEqual = f4.notEqual;
    var f4greaterThan = f4.greaterThan;
    var f4greaterThanOrEqual = f4.greaterThanOrEqual;

    var f4select = f4.select;

    var fround = stdlib.Math.fround;

    var globImportF4 = f4check(imports.g1);       // global var import
    var globImportB4 = b4check(imports.g2);       // global var import
    

    var f4g1 = f4(-5033.2,-3401.0,665.34,32234.1);          // global var initialized
    var f4g2 = f4(1194580.33,-11201.5,63236.93,334.8);          // global var initialized

    var i4g1 = i4(1065353216, -1073741824, -1077936128, 1082130432);          // global var initialized
    var i4g2 = i4(353216, -492529, -1128, 1085);          // global var initialized

    var gval = 1234;
    var gval2 = 1234.0;
    
    var loopCOUNT = 3;

    function func1(a)
    {
        a = a|0;
        var b = f4(5033.2,-3401.0,665.34,-32234.1);
        var c = f4(-34183.8985, 212344.12, 665.34, 65534.99);
        var d = b4(0,-1,0,-1);


        var loopIndex = 0;
        while ( (loopIndex|0) < (loopCOUNT|0)) {

            b = f4select(d, b, c);
            

            loopIndex = (loopIndex + 1) | 0;
        }

        return f4check(b);
    }
    
    function func2(a)
    {
        a = a|0;
        var b = f4(5033.2,-3401.0,665.34,-32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = b4(0,-1,0,-1);

        var loopIndex = 0;
        for (loopIndex = 0; (loopIndex | 0) < (loopCOUNT | 0) ; loopIndex = (loopIndex + 1) | 0)
        {

            b = f4select(d, f4g1, f4g2);
                

        }

        return f4check(b);
    }

    function func3(a)
    {
        a = a|0;
        var b = f4(5033.2,-3401.0,665.34,-32234.1);
        var c = f4(-34183.8985, 212344.12, -569437.0, 65534.99);
        var d = b4(0,-1,0,-1);


        var loopIndex = 0;
        loopIndex = loopCOUNT | 0;
        do {

            globImportF4 = f4select(d, globImportF4, f4g2);
    

            loopIndex = (loopIndex - 1) | 0;
        }
        while ( (loopIndex | 0) > 0);

        return f4check(globImportF4);
    }
    
    
    
    return {func1:func1, func2:func2, func3:func3};
}

var m = asmModule(this, {g1:SIMD.Float32x4(90934.2,123.9,419.39,449.0), g2:SIMD.Bool32x4(-1065353216, -1073741824,-1077936128, -1082130432) /*, g3:SIMD.Float64x2(110.20, 58967.0, 14511.670, 191766.23431)*/});

var ret;
ret = m.func1();
equalSimd([-34183.8984375, -3401, 665.3400268554687, -32234.099609375], ret, SIMD.Float32x4, "func1");

ret = m.func2();
equalSimd([1194580.375, -3401, 63236.9296875, 32234.099609375], ret, SIMD.Float32x4, "func2");

ret = m.func3();
equalSimd([1194580.375, 123.9000015258789, 63236.9296875, 449], ret, SIMD.Float32x4, "func3");

print("PASS");



