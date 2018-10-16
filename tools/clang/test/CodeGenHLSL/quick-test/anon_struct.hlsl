// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s

// CHECK: %"$Globals" = type { %struct.anon }
// CHECK: @dx.op.cbufferLoadLegacy

struct {
    int X;
} CB;

float main(int N : A, int C : B) : SV_TARGET {
    return CB.X;
}
