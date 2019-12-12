// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:         [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:    [[varNameC:%\d+]] = OpString "c"
// CHECK: [[varNameCond:%\d+]] = OpString "cond"
// CHECK:    [[varNameA:%\d+]] = OpString "a"
// CHECK:    [[varNameB:%\d+]] = OpString "b"

// CHECK:      [[source:%\d+]] = OpExtInst %void [[set]] DebugSource %18 %19
// CHECK: [[compileUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit 1 4 [[source]] HLSL

// CHECK: [[mainFnLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock [[source]] 29 1 [[compileUnit]]
// CHECK:                {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameC]] [[float4Type:%\d+]] [[source]] 30 10 [[mainFnLexBlock]] FlagIsLocal
// CHECK:                {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameCond]] [[boolType:%\d+]] [[source]] 31 8 [[mainFnLexBlock]] FlagIsLocal

// CHECK: [[ifLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock [[source]] 34 13 [[mainFnLexBlock]]
// CHECK:            {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameA]] [[intType:%\d+]] [[source]] 35 9 [[ifLexBlock]] FlagIsLocal

// CHECK: [[tempLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock [[source]] 37 5 [[ifLexBlock]]
// CHECK:              {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameB]] [[uintType:%\d+]] [[source]] 38 12 [[tempLexBlock]] FlagIsLocal

// CHECK: [[floatType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK:     [[float4Type]] = OpExtInst %void [[set]] DebugTypeVector [[floatType]] 4
// CHECK:       [[boolType]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean
// CHECK:        [[intType]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed
// CHECK:       [[uintType]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Unsigned

float4 main(float4 color : COLOR) : SV_TARGET
{
  float4 c = 0.xxxx;
  bool cond = c.x == 0;


  if (cond) {
    int a = 2;
    c = c + c;
    {
      uint b = 3;
      c = c + c;
    }
  }


  return color + c;
}


