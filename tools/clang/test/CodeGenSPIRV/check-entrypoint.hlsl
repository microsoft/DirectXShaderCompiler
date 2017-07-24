// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability Shader
// CHECK-NOT: OpCapability Kernel
// CHECK-NEXT: OpMemoryModel Logical GLSL450
void main()
// CHECK-NEXT: OpEntryPoint Fragment %main "main"
{

}
