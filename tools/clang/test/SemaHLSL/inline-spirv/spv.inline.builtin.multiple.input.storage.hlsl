// RUN: %dxc -T cs_6_9 -E main -ast-dump -spirv %s | FileCheck %s

[[vk::ext_storage_class(/* Input */ 1)]]
[[vk::ext_builtin_input(/* WorkgroupId */ 26)]]
static uint3 input;
// CHECK: VarDecl 0x{{.+}} <{{.+}}> col:14 input 'uint3':'vector<unsigned int, 3>' static
// CHECK-NEXT: VKExtBuiltinInputAttr 0x{{.+}} <line:4:3> 26

[numthreads(1, 1, 1)]
void main() { }
