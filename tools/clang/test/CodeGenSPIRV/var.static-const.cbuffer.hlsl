// RUN: %dxc -T ps_6_0 -E main %s -spirv 2>&1 | FileCheck %s

cbuffer MetadataCB {
    uint m0; static const int c0 = 0;
    uint m1; static const int c1 = 0;
    uint m2; static const int c2 = 0;
};

float4 main() : SV_Target {
    return float4(asfloat(m2), 0, 0, 0);
}

// CHECK: warning: cbuffer member initializer ignored since no Vulkan equivalent
// CHECK: warning: cbuffer member initializer ignored since no Vulkan equivalent
// CHECK: warning: cbuffer member initializer ignored since no Vulkan equivalent
// CHECK: OpName %type_MetadataCB "type.MetadataCB"
// CHECK-NEXT: OpMemberName %type_MetadataCB 0 "m0"
// CHECK-NEXT: OpMemberName %type_MetadataCB 1 "m1"
// CHECK-NEXT: OpMemberName %type_MetadataCB 2 "m2"
// CHECK: %type_MetadataCB = OpTypeStruct %uint %uint %uint
// CHECK: OpAccessChain %_ptr_Uniform_uint %MetadataCB %int_2
