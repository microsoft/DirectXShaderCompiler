// RUN: %dxc -T vs_6_9 -E main -ast-dump -spirv %s | FileCheck %s

[[vk::ext_decorate(/* Location */ 30, 0)]]
static float4 output;

// CHECK: VarDecl 0x{{.+}} <{{.+}}> col:15 output 'float4':'vector<float, 4>' static
// CHECK-NEXT: VKLocationAttr 0x{{.+}} <line:3:3> 0

void main() {}
