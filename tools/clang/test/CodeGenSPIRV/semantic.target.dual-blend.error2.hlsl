// Run: %dxc -T vs_6_0 -E main

int main(
    [[vk::index(1)]] int a : A
) : B {
    return a;
}

// CHECK: :4:7: error: vk::index only allowed in pixel shader
// CHECK: :4:7: error: vk::index should be used together with vk::location for dual-source blending
