// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:   [[debugSource:%\d+]] = OpExtInst %void [[debugSet]] DebugSource
// CHECK:          [[main:%\d+]] = OpExtInst %void [[debugSet]] DebugFunction
// CHECK: [[mainFnLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 13 1 [[main]]
// CHECK: [[whileLoopLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 21 3 [[mainFnLexBlock]]
// CHECK: [[ifStmtLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 26 20 [[whileLoopLexBlock]]
// CHECK: [[tempLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 28 7 [[ifStmtLexBlock]]
// CHECK: [[forLoopLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 15 12 [[mainFnLexBlock]]

float4 main(float4 color : COLOR) : SV_TARGET
{
  float4 c = 0.xxxx;
  for (;;) {
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;
  }
  while (c.x)
  {
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;

    if (bool(c.x)) {
      c = c + c;
      {
        c = c + c;
      }
    }
  }

  return color + c;
}

