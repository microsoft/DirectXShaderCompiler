// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s

struct S {
    float  a;
    float2 b;
    float  c;
};

SubpassInput SI0; // error

void main() {

}

// CHECK:  :9:14: error: missing vk::input_attachment_index attribute
