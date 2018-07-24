// RUN: %dxc -E main -T ps_6_0 -Zi %s | FileCheck %s

// CHECK: warning: min12int is promoted to min16int
// CHECK: define void @main
// CHECK: ret void

[RootSignature("")]
float2 main( min10float mf:P , int s:Q) : SV_Target
{
    if(s > 1)
    {
        min10float new_mf = mf * mf;
        new_mf = new_mf / 2.0;
        float f = float(new_mf);
        f = (f * 4.0) / 3.3;
        min10float new_mf1 = min10float(f);
        new_mf1 = new_mf1 + new_mf;
        return float2(new_mf1, new_mf1);
    }
    return float2(1.0, 1.0);
}