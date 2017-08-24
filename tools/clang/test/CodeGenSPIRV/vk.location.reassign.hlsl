// Run: %dxc -T vs_6_0 -E main

struct S {
    [[vk::location(3)]] float a : A;  // error
    [[vk::location(3)]] float b : B;  // error
};

struct T {
    [[vk::location(3)]] float i : A;  // error
    [[vk::location(3)]] float j : B;  // error
};

[[vk::location(3)]]                   // first use
float main(
    [[vk::location(3)]]     float m : M,  // first use
    [[vk::location(3)]]     float n : N,  // error
                            S     s,

    [[vk::location(3)]] out float x : X,  // error
    [[vk::location(3)]] out float y : Y,  // error
                        out T     t
) : R {
    return 1.0;
}

// CHECK: 16:7: error: stage input location #3 already assigned
// CHECK: 4:7: error: stage input location #3 already assigned
// CHECK: 5:7: error: stage input location #3 already assigned

// CHECK: 19:7: error: stage output location #3 already assigned
// CHECK: 20:7: error: stage output location #3 already assigned
// CHECK: 9:7: error: stage output location #3 already assigned
// CHECK: 10:7: error: stage output location #3 already assigned
