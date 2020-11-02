// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK: [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[y:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 26 23 {{%\d+}} FlagIsLocal 2
// CHECK: [[x:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 26 14 {{%\d+}} FlagIsLocal 1
// CHECK: [[condition:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 33 8 {{%\d+}} FlagIsLocal
// CHECK: [[expr:%\d+]] = OpExtInst %void [[set]] DebugExpression
// CHECK: [[color:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 31 20 {{%\d+}} FlagIsLocal 1

// CHECK:     %color = OpFunctionParameter
// CHECK:   {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[color]] %color [[expr]]
// CHECK: %condition = OpVariable
// CHECK:              OpLine {{%\d+}} 33 8
// CHECK:              OpStore %condition %false
// CHECK:   {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[condition]] %condition [[expr]]

// CHECK:            %x = OpFunctionParameter
// CHECK:            %y = OpFunctionParameter
// CHECK:                 OpLine {{%\d+}} 26 14
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[x]] %x [[expr]]
// CHECK-NEXT:            OpLine {{%\d+}} 26 23
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[y]] %y [[expr]]

void foo(int x, float y)
{
  x = x + y;
}

float4 main(float4 color : COLOR) : SV_TARGET
{
  bool condition = false;
  foo(1, color.x);
  return color;
}

