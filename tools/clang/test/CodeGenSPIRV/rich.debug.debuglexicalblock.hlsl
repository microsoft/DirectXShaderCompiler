// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich -fcgl  %s -spirv | FileCheck %s

// CHECK:      [[debugSet:%[0-9]+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:   [[debugSource:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugSource
// CHECK:          [[main:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugFunction
// CHECK: [[mainFnLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 16 1 [[main]]
// CHECK: [[whileLoopLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 24 3 [[mainFnLexBlock]]
// CHECK: [[ifStmtLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 29 20 [[whileLoopLexBlock]]
// CHECK: [[tempLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 31 7 [[ifStmtLexBlock]]
// CHECK: [[forLoopParensLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 18 3 [[mainFnLexBlock]]
// CHECK: [[forLoopLexBlock:%[0-9]+]] = OpExtInst %void [[debugSet]] DebugLexicalBlock [[debugSource]] 18 31 [[forLoopParensLexBlock]]
// CHECK: {{%[0-9]+}} = OpExtInst %void [[debugSet]] DebugLocalVariable {{%[0-9]+}} {{%[0-9]+}} [[debugSource]] 19 12 [[forLoopLexBlock]] FlagIsLocal
// CHECK: {{%[0-9]+}} = OpExtInst %void [[debugSet]] DebugLocalVariable {{%[0-9]+}} {{%[0-9]+}} [[debugSource]] 18 12 [[forLoopParensLexBlock]] FlagIsLocal

float4 main(float4 color : COLOR) : SV_TARGET
{
  float4 c = 0.xxxx;
  for (int i = 0; i < 4; ++i) {
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b + i;
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

