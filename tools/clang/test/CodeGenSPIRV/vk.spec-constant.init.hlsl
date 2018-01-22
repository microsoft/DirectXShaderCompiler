// Run: %dxc -T vs_6_0 -E main

// CHECK: OpDecorate [[b0:%\d+]] SpecId 0
// CHECK: OpDecorate [[b1:%\d+]] SpecId 1
// CHECK: OpDecorate [[b2:%\d+]] SpecId 2

// CHECK: OpDecorate [[i0:%\d+]] SpecId 10
// CHECK: OpDecorate [[i1:%\d+]] SpecId 11
// CHECK: OpDecorate [[i2:%\d+]] SpecId 12
// CHECK: OpDecorate [[i3:%\d+]] SpecId 13

// CHECK: OpDecorate [[u0:%\d+]] SpecId 20

// CHECK: OpDecorate [[f0:%\d+]] SpecId 30
// CHECK: OpDecorate [[f1:%\d+]] SpecId 31
// CHECK: OpDecorate [[f2:%\d+]] SpecId 32
// CHECK: OpDecorate [[f3:%\d+]] SpecId 33

// CHECK: [[b0]] = OpSpecConstantTrue %bool
[[vk::constant_id(0)]]
bool b0 = true;
// CHECK: [[b1]] = OpSpecConstantFalse %bool
[[vk::constant_id(1)]]
bool b1 = 0;
// CHECK: [[b2]] = OpSpecConstantTrue %bool
[[vk::constant_id(2)]]
bool b2 = 1.5;


// CHECK:  [[i0]] = OpSpecConstant %int 42
[[vk::constant_id(10)]]
int i0 = 42;
// CHECK:  [[i1]] = OpSpecConstant %int -42
[[vk::constant_id(11)]]
int i1 = -42;
// CHECK:  [[i2]] = OpSpecConstant %int 1
[[vk::constant_id(12)]]
int i2 = (true);
// CHECK:  [[i3]] = OpSpecConstant %int 2
[[vk::constant_id(13)]]
int i3 = 2.5;

// CHECK: [[u0]] = OpSpecConstant %uint 56
[[vk::constant_id(20)]]
uint uintConst1 = 56;

// CHECK: [[f0]] = OpSpecConstant %float 4.2
[[vk::constant_id(30)]]
float f0 = (4.2);
// CHECK: [[f1]] = OpSpecConstant %float -4.2
[[vk::constant_id(31)]]
float f1 = -4.2;
// CHECK: [[f2]] = OpSpecConstant %float 1
[[vk::constant_id(32)]]
float f2 = true;
// CHECK: [[f3]] = OpSpecConstant %float 20
[[vk::constant_id(33)]]
float f3 = 20;


float main() : A {
    return 1.0;
}
