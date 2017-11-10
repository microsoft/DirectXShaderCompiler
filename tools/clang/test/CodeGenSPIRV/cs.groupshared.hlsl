// Run: %dxc -T cs_6_0 -E main

struct S {
    float  f1;
    float3 f2;
};

// CHECK: %a = OpVariable %_ptr_Workgroup_float Workgroup
groupshared              float    a;
// CHECK: %b = OpVariable %_ptr_Workgroup_v3float Workgroup
groupshared              float3   b;
// CHECK: %c = OpVariable %_ptr_Workgroup_mat2v3float Workgroup
groupshared column_major float2x3 c;
// CHECK: %d = OpVariable %_ptr_Workgroup__arr_v2float_uint_5 Workgroup
groupshared              float2   d[5];
// CHECK: %s = OpVariable %_ptr_Workgroup_S Workgroup
groupshared              S        s;

[numthreads(8, 8, 8)]
void main(uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID) {
// Make sure pointers have the correct storage class
// CHECK:    {{%\d+}} = OpAccessChain %_ptr_Workgroup_float %s %int_0
// CHECK: [[d0:%\d+]] = OpAccessChain %_ptr_Workgroup_v2float %d %int_0
// CHECK:    {{%\d+}} = OpAccessChain %_ptr_Workgroup_float [[d0]] %int_1
    d[0].y = s.f1;
}
