// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:         [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:    [[varNameC:%\d+]] = OpString "c"
// CHECK: [[varNameCond:%\d+]] = OpString "cond"

// CHECK:      [[source:%\d+]] = OpExtInst %void [[set]] DebugSource {{%\d+}} {{%\d+}}
// CHECK: [[compileUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit 1 4 [[source]] HLSL

// CHECK: [[floatType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK: [[float4Type:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[floatType]] 4
// CHECK:  [[boolType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable [[varNameC]] [[float4Type]] [[source]] 17 15 [[compileUnit]] [[varNameC]] %c FlagIsDefinition
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable [[varNameCond]] [[boolType]] [[source]] 18 13 [[compileUnit]] [[varNameCond]] %cond FlagIsDefinition

static float4 c;
static bool cond;

float4 main(float4 color : COLOR) : SV_TARGET {
  c = 0.xxxx;
  cond = c.x == 0;

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
