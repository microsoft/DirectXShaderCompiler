// RUN: %dxc -T vs_6_9 -E main -ast-dump -spirv %s | FileCheck %s

[[vk::ext_storage_class(/* Output */ 3)]]
[[vk::ext_builtin_output(/* Position */ 0)]]
static float4 output;
// CHECK: VarDecl 0x{{.+}} <{{.+}}> col:15 output 'float4':'vector<float, 4>' static
// CHECK-NEXT: VKExtBuiltinOutputAttr 0x{{.+}} <line:4:3> 0

void main() {
}
