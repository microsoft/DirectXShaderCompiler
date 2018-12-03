// RUN: %dxc -E main -T ps_6_0 -O0 %s | FileCheck %s

// Ensure that bools are converted from/to their memory representation when loaded/stored

// Local variables should never be i1s
// CHECK-not: alloca {{.*}}i1

// Test stores
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: store i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: store i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: store i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: store i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: store i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: store i32

// Test loads
// CHECK: load i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: load i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: load i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: load i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: load i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: load i32
// CHECK: icmp ne i32 {{.*}}, 0

float main(int i : I) : SV_Target
{
    bool s = i == 42;
    bool1 v = i == 42;
    bool1x1 m = i == 42;
    bool sa[1] = { i == 42 };
    bool1 va[1] = { i == 42 };
    bool1x1 ma[1] = { i == 42 };

    return s && v.x && m._11 && sa[0] && va[0].x && ma[0]._11 ? 1.0f : 2.0f;
}