// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:  [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[compUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit
// CHECK: [[main:%\d+]] = OpExtInst %void [[set]] DebugFunction
// CHECK: [[mainFnLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%\d+}} 15 1 [[main]]
// CHECK: [[whileLoopLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%\d+}} 38 3 [[mainFnLexBlock]]
// CHECK: [[ifStmtLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%\d+}} 45 20 [[whileLoopLexBlock]]
// CHECK: [[tempLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%\d+}} 50 7 [[ifStmtLexBlock]]
// CHECK: [[forLoopLexBlock:%\d+]] = OpExtInst %void [[set]] DebugLexicalBlock {{%\d+}} 23 12 [[mainFnLexBlock]]

float4 main(float4 color : COLOR) : SV_TARGET
// CHECK:     %src_main = OpFunction
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[main]]
{
// CHECK:     %bb_entry = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]

  float4 c = 0.xxxx;

// CHECK:    %for_check = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  for (;;) {
// CHECK:     %for_body = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[forLoopLexBlock]]
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;
// CHECK: %for_continue = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  }
// CHECK:    %for_merge = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]

// CHECK:  %while_check = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  while (c.x)
  {
// CHECK:   %while_body = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[whileLoopLexBlock]]
    float4 a = 0.xxxx;
    float4 b = 1.xxxx;
    c = c + a + b;

    if (bool(c.x)) {
// CHECK:      %if_true = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[ifStmtLexBlock]]
      c = c + c;
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[tempLexBlock]]
      {
        c = c + c;
      }
    }
// CHECK:     %if_merge = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[whileLoopLexBlock]]

// CHECK:%while_continue = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]
  }
// CHECK:  %while_merge = OpLabel
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugScope [[mainFnLexBlock]]

  return color + c;
}

