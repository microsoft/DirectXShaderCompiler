// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich -fcgl  %s -spirv | FileCheck %s

// CHECK:  [[set:%[0-9]+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[compUnit:%[0-9]+]] = OpExtInst %void [[set]] DebugCompilationUnit
// CHECK: [[main:%[0-9]+]] = OpExtInst %void [[set]] DebugFunction
// CHECK: [[mainFnLexBlock:%[0-9]+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%[0-9]+}} 17 1 [[main]]
// CHECK: [[whileLoopLexBlock:%[0-9]+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%[0-9]+}} 37 3 [[mainFnLexBlock]]
// CHECK: [[ifStmtLexBlock:%[0-9]+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%[0-9]+}} 44 20 [[whileLoopLexBlock]]
// CHECK: [[tempLexBlock:%[0-9]+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%[0-9]+}} 49 7 [[ifStmtLexBlock]]
// CHECK: [[forLoopLexBlock:%[0-9]+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%[0-9]+}} 22 12 [[mainFnLexBlock]]
// CHECK: [[srcMain:%[0-9]+]] = OpExtInst %void [[set]] DebugFunction

float4 main(float4 color : COLOR) : SV_TARGET
// CHECK:     %main = OpFunction
// CHECK:     %src_main = OpFunction
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[srcMain]]
{
// CHECK:     %bb_entry = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]

  float4 c = 0.xxxx;
  for (;;) {
// CHECK:     %for_body = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[forLoopLexBlock]]
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;
// CHECK: %for_continue = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  }
// CHECK:    %for_merge = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]

// CHECK:  %while_check = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  while (c.x)
  {
// CHECK:   %while_body = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[whileLoopLexBlock]]
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;

    if (bool(c.x)) {
// CHECK:      %if_true = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[ifStmtLexBlock]]
      c = c + c;
// CHECK: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[tempLexBlock]]
      {
        c = c + c;
      }
    }
// CHECK:     %if_merge = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[whileLoopLexBlock]]

// CHECK:%while_continue = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  }
// CHECK:  %while_merge = OpLabel
// CHECK-NEXT: {{%[0-9]+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]

  return color + c;
}

