// RUN: %dxc -T cs_6_9 -E main -ast-dump -spirv %s | FileCheck %s

[[vk::ext_decorate(/* BuiltIn */ 11, /* WorkgroupId */ 26)]]
[[vk::ext_builtin_output(/* WorkgroupId */ 26)]]
static uint3 input;

// CHECK: VarDecl 0x{{.+}} <{{.+}}> col:14 input 'uint3':'vector<unsigned int, 3>' static
// CHECK-NEXT: VKExtBuiltinOutputAttr 0x{{.+}} <line:3:3> 26

[numthreads(1, 1, 1)]
void main() { }
