// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK:   [[debugSource:%\d+]] = OpExtInst %void [[debugSet]] DebugSource
// CHECK:          [[main:%\d+]] = OpExtInst %void [[debugSet]] DebugFunction
// CHECK: [[mainFnLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_13 %uint_1 [[main]]
// CHECK: [[whileLoopLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_21 %uint_3 [[mainFnLexBlock]]
// CHECK: [[ifStmtLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_26 %uint_20 [[whileLoopLexBlock]]
// CHECK: [[tempLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_28 %uint_7 [[ifStmtLexBlock]]
// CHECK: [[forLoopLexBlock:%\d+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_15 %uint_12 [[mainFnLexBlock]]

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

