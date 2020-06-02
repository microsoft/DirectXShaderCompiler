// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:         [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:    [[varNameB:%\d+]] = OpString "b"
// CHECK:    [[varNameA:%\d+]] = OpString "a"
// CHECK: [[varNameCond:%\d+]] = OpString "cond"
// CHECK:    [[varNameC:%\d+]] = OpString "c"

// CHECK:  [[uintType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Unsigned
// CHECK: [[floatType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK: [[float4Type:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[floatType]] 4
// CHECK:      [[source:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[compileUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit 1 4 [[source]] HLSL
// CHECK:      [[main:%\d+]] = OpExtInst %void [[set]] DebugFunction {{%\d+}} {{%\d+}} [[source]] 29 1 [[compileUnit]] {{%\d+}} FlagIsProtected|FlagIsPrivate 30 %src_main

// CHECK: [[mainFnLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock [[source]] 30 1 [[main]]
// CHECK: [[ifLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock [[source]] 35 13 [[mainFnLexBlock]]
// CHECK: [[tempLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock [[source]] 38 5 [[ifLexBlock]]

// CHECK:              {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameB]] [[uintType]] [[source]] 39 12 [[tempLexBlock]] FlagIsLocal

// CHECK:   [[intType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed
// CHECK:            {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameA]] [[intType]] [[source]] 36 9 [[ifLexBlock]] FlagIsLocal

// CHECK:  [[boolType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean
// CHECK:                {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameCond]] [[boolType]] [[source]] 32 8 [[mainFnLexBlock]] FlagIsLocal
// CHECK:                {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[varNameC]] [[float4Type]] [[source]] 31 10 [[mainFnLexBlock]] FlagIsLocal

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


