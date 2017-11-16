// Run: %dxc -T ps_6_0 -E main

struct S {
    float f;
};

[[vk::push_constant, vk::binding(5)]]
S pcs;

float main() : A {
    return 1.0;
}

// CHECK: :7:3: error: 'push_constant' attribute cannot be used together with 'binding' attribute
