// Run: %dxc -T vs_6_0 -E main

struct S {
    float f;
};

[[vk::shader_record_nv, vk::binding(6)]]
ConstantBuffer<S> recordBuf;

float main() : A {
    return 1.0;
}

// CHECK: :7:3: error: vk::shader_record_nv attribute cannot be used together with vk::binding attribute