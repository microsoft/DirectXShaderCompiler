// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK: [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[expr:%\d+]] = OpExtInst %void [[set]] DebugExpression

// CHECK: [[color:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 30 20 {{%\d+}} FlagIsLocal 1
// CHECK: [[condition:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 32 8 {{%\d+}} FlagIsLocal

// CHECK: [[x:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 25 14 {{%\d+}} FlagIsLocal 1
// CHECK: [[y:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 25 23 {{%\d+}} FlagIsLocal 2

// CHECK:        %color = OpFunctionParameter
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[color]] %color [[expr]]
// CHECK:      %condition = OpVariable
// CHECK:                 OpStore %condition %false
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[condition]] %condition [[expr]]

// CHECK:            %x = OpFunctionParameter
// CHECK:            %y = OpFunctionParameter
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[x]] %x [[expr]]
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

