// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan -fcgl  %s -spirv | FileCheck %s

// CHECK:      [[debugSet:%[0-9]+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK:   [[debugSource:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugSource
// CHECK:          [[main:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugFunction
// CHECK: [[mainFnLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_14 %uint_1 [[main]]
// CHECK: [[whileLoopLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_22 %uint_3 [[mainFnLexBlock]]
// CHECK: [[ifStmtLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_27 %uint_20 [[whileLoopLexBlock]]
// CHECK: [[tempLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_29 %uint_7 [[ifStmtLexBlock]]
// CHECK: [[forLoopParensLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_16 %uint_3 [[mainFnLexBlock]]
// CHECK: [[forLoopLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_16 %uint_12 [[forLoopParensLexBlock]]

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

