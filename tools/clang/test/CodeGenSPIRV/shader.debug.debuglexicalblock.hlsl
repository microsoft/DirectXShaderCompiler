// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan -fcgl  %s -spirv | FileCheck %s

// CHECK:          [[debugSet:%[0-9]+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK:               [[foo:%[0-9]+]] = OpString "foo"
// CHECK:       [[debugSource:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugSource
// CHECK:              [[main:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugFunction
// CHECK:    [[mainFnLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_17 %uint_1 [[main]]
// CHECK: [[whileLoopLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_25 %uint_3 [[mainFnLexBlock]]
// CHECK:    [[ifStmtLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_30 %uint_20 [[whileLoopLexBlock]]
// CHECK:      [[tempLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_32 %uint_7 [[ifStmtLexBlock]]
// CHECK:   [[forLoopLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] %uint_19 %uint_12 [[mainFnLexBlock]]
// CHECK:         [[globalVar:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugGlobalVariable [[foo]] %25 [[debugSource]] %uint_14 %uint_15

uniform float foo;

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

  return color + c * foo;
}

