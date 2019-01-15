// RUN: %dxc -E main -T vs_6_0 -O0 %s | FileCheck %s

// Ensure that bools are converted from/to their memory representation when loaded/stored
// in local variables.

// Local variables should never be i1s
// CHECK-not: alloca {{.*}}i1

int main(int i : I) : OUT
{
    // CHECK: icmp eq i32 {{.*}}, 42
    // CHECK: zext i1 {{.*}} to i32
    // CHECK: store i32
    bool s = i == 42;
    // CHECK: icmp eq i32 {{.*}}, 42
    // CHECK: zext i1 {{.*}} to i32
    // CHECK: store i32
    bool1 v = i == 42;
    // CHECK: icmp eq i32 {{.*}}, 42
    // CHECK: zext i1 {{.*}} to i32
    // CHECK: store i32
    bool1x1 m = i == 42;
    // CHECK: icmp eq i32 {{.*}}, 42
    // CHECK: zext i1 {{.*}} to i32
    // CHECK: store i32
    bool sa[1] = { i == 42 };
    // CHECK: icmp eq i32 {{.*}}, 42
    // CHECK: zext i1 {{.*}} to i32
    // CHECK: store i32
    bool1 va[1] = { i == 42 };
    // CHECK: icmp eq i32 {{.*}}, 42
    // CHECK: zext i1 {{.*}} to i32
    // CHECK: store i32
    bool1x1 ma[1] = { i == 42 };

    // CHECK: load i32
    // CHECK: icmp ne i32 {{.*}}, 0
    return (s
        // CHECK: load i32
        // CHECK: icmp ne i32 {{.*}}, 0
        && v.x
        // CHECK: load i32
        // CHECK: icmp ne i32 {{.*}}, 0
        && m._11
        // CHECK: load i32
        // CHECK: icmp ne i32 {{.*}}, 0
        && sa[0]
        // CHECK: load i32
        // CHECK: icmp ne i32 {{.*}}, 0
        && va[0].x
        // CHECK: load i32
        // CHECK: icmp ne i32 {{.*}}, 0
        && ma[0]._11) ? 1 : 2;
}