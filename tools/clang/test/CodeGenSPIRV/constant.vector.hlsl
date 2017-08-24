// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK: %int_1 = OpConstant %int 1
    int1 c_int1 = int1(1);
// CHECK-NEXT: %int_2 = OpConstant %int 2
// CHECK-NEXT: {{%\d+}} = OpConstantComposite %v2int %int_1 %int_2
    int2 c_int2 = int2(1, 2);
// CHECK-NEXT: %int_3 = OpConstant %int 3
// CHECK-NEXT: {{%\d+}} = OpConstantComposite %v3int %int_1 %int_2 %int_3
    int3 c_int3 = int3(1, 2, 3);
// CHECK-NEXT: %int_4 = OpConstant %int 4
// CHECK-NEXT: {{%\d+}} = OpConstantComposite %v4int %int_1 %int_2 %int_3 %int_4
    int4 c_int4 = int4(1, 2, 3, 4);
}
