// Run: %dxc -T vs_6_0 -E main

struct S {
    float f;
};

[[vk::push_constant, vk::binding(5)]]
S pcs;

[[vk::shader_record_nv, vk::binding(6)]]
ConstantBuffer<S> recordBuf;

float main() : A {
    return 1.0;
}

// CHECK: :7:3: error: vk::push_constant attribute cannot be used together with vk::binding attribute
// CHECK: :10:3: error: vk::shader_record_nv attribute cannot be used together with vk::binding attribute