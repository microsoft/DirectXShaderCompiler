// Run: %dxc -T lib_6_3 -fspv-extension=SPV_NV_ray_tracing

struct T {
    float2 val[3];
};

// CHECK: OpName %type_ShaderRecordBufferNV_S "type.ShaderRecordBufferNV.S"
// CHECK: OpMemberName %type_ShaderRecordBufferNV_S 0 "f1"
// CHECK: OpMemberName %type_ShaderRecordBufferNV_S 1 "f2"
// CHECK: OpMemberName %type_ShaderRecordBufferNV_S 2 "f3"
// CHECK: OpMemberName %type_ShaderRecordBufferNV_S 3 "f4"
// CHECK-NOT: OpDecorate %srb DescriptorSet
// CHECK-NOT: OpDecorate %srb Binding

// CHECK: %type_ShaderRecordBufferNV_S = OpTypeStruct %float %v3float %mat2v3float %T
struct S {
    float    f1;
    float3   f2;
    float2x3 f3;
    T        f4;
};
// CHECK: %_ptr_ShaderRecordBufferNV_type_ShaderRecordBufferNV_S = OpTypePointer ShaderRecordBufferNV %type_ShaderRecordBufferNV_S

// CHECK: %srb = OpVariable %_ptr_ShaderRecordBufferNV_type_ShaderRecordBufferNV_S ShaderRecordBufferNV
[[vk::shader_record_nv]]
ConstantBuffer<S> srb;

struct Payload { float p; };
struct Attribute { float a; };

[shader("miss")]
void main(inout Payload P) 
{
   P.p = 
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_ShaderRecordBufferNV_float %srb %int_0
        srb.f1 +
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_ShaderRecordBufferNV_v3float %srb %int_1
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_ShaderRecordBufferNV_float [[ptr]] %int_2
        srb.f2.z +
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_ShaderRecordBufferNV_float %srb %int_2 %uint_1 %uint_2
        srb.f3[1][2] +
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_ShaderRecordBufferNV_v2float %srb %int_3 %int_0 %int_2
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_ShaderRecordBufferNV_float [[ptr]] %int_1
        srb.f4.val[2].y;
}
