// Run: %dxc -T vs_6_0 -E main

float main(
    [[vk::location(5)]] int   a: A,
                        float b: B
) : R { return 1.0; }

// CHECK:      error: partial explicit stage input location assignment via
// CHECK-SAME: vk::location(X)
// CHECK-SAME: unsupported
