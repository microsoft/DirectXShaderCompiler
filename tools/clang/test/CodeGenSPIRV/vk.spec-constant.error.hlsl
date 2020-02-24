// Run: %dxc -T vs_6_0 -E main

[[vk::constant_id(0)]]
const bool sc0 = true;

[[vk::constant_id(1)]]
const bool sc1 = sc0; // error

[[vk::constant_id(2)]]
const double sc2 = 42; // error

[[vk::constant_id(3)]]
const int sc3; // error

[[vk::constant_id(4)]]
const int sc4 = sc3 + sc3; // error

[[vk::constant_id(5)]]
static const int sc5 = 1; // error

float main() : A {
    return 1.0;
}

// CHECK: :7:18: error: unsupported specialization constant initializer
