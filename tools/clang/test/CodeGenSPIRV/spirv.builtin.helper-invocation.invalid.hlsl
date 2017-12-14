// Run: %dxc -T vs_6_0 -E main

[[vk::location(5), vk::builtin("HelperInvocation")]]
int main() : A
{
    return 1;
}

// CHECK: :3:20: error: cannot use vk::builtin and vk::location together
// CHECK: :3:20: error: HelperInvocation builtin must be of boolean type
// CHECK: :3:20: error: HelperInvocation builtin can only be used as pixel shader input
