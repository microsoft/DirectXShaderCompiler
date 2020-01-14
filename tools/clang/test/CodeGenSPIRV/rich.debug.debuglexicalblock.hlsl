// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:   [[debugSource:%\d+]] = OpExtInst %void [[debugSet]] DebugSource {{%\d+}} {{%\d+}}
// CHECK:      [[compUnit:%\d+]] = OpExtInst %void [[debugSet]] DebugCompilationUnit 1 4 [[debugSource]] HLSL
float4 main(float4 color : COLOR) : SV_TARGET
// CHECK:          [[main:%\d+]] = OpExtInst %void [[debugSet]] DebugFunction {{%\d+}} {{%\d+}} [[debugSource]] 6 1 [[compUnit]] {{%\d+}} FlagIsProtected|FlagIsPrivate 9 %src_main
// CHECK: [[mainFnLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 9 1 [[main]]
{
  float4 c = 0.xxxx;
  // CHECK: [[forLoopLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 12 12 [[mainFnLexBlock]]
  for (;;) {
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;
  }
  while (c.x)
  // CHECK: [[whileLoopLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 19 3 [[mainFnLexBlock]]
  {
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;

    // CHECK: [[ifStmtLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 25 20 [[whileLoopLexBlock]]
    if (bool(c.x)) {
      c = c + c;
      // CHECK: [[tempLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 28 7 [[ifStmtLexBlock]]
      {
        c = c + c;
      }
    }
  }

  return color + c;
}

