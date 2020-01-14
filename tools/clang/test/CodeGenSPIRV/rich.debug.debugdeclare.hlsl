// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"

// CHECK: [[color:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 28 20 {{%\d+}} FlagIsLocal 0
// CHECK: [[condition:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 30 8 {{%\d+}} FlagIsLocal

// CHECK: [[x:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 23 14 {{%\d+}} FlagIsLocal 0
// CHECK: [[y:%\d+]] = OpExtInst %void [[set]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 23 23 {{%\d+}} FlagIsLocal 1

// CHECK:        %color = OpFunctionParameter
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[color]] %color
// CHECK:      %condition = OpVariable
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[condition]] %condition

// CHECK:            %x = OpFunctionParameter
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[x]] %x
// CHECK:            %y = OpFunctionParameter
// CHECK-NEXT: {{%\d+}} = OpExtInst %void [[set]] DebugDeclare [[y]] %y

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

