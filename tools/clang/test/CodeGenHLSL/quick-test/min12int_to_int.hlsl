// RUN: %dxc -E main -T ps_6_0 -Zi %s | FileCheck %s

// CHECK: warning: min12int is promoted to min16int
// CHECK: define void @main
// CHECK: ret void

[RootSignature("")]
int2 main( min12int mi:P , int s:Q) : SV_Target
{
    if(s > 1)
    {
        min12int new_mi = mi * mi;
        new_mi = min12int(new_mi * 2);
        int f = int(new_mi);
        f = (f * 4) / 3;
        min12int new_mi1 = min12int(f);
        new_mi1 = new_mi1 + new_mi;
        return int2(new_mi1, new_mi1);
    }
    return int2(1, 1);
}