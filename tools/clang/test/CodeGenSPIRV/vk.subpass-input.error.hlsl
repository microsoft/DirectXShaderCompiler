// Run: %dxc -T ps_6_0 -E main

struct S {
    float  a;
    float2 b;
    float  c;
};

SubpassInput SI0; // error

[[vk::input_attachment_index(0)]]
SubpassInput<S> SI1; // error

[[vk::input_attachment_index(1)]]
static SubpassInput SI2; // error

void main() {

}

// CHECK:  :9:14: error: missing vk::input_attachment_index attribute
// CHECK: :12:17: error: only scalar/vector types allowed as SubpassInput(MS) parameter type
// CHECK: :15:21: error: SubpassInput(MS) must be externally visible