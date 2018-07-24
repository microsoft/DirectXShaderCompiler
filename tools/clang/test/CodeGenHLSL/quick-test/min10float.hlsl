// RUN: %dxc -E main -T ps_6_0 -Zi %s | FileCheck %s

// CHECK: warning: min10float is promoted to min16float
// CHECK: define void @main
// CHECK: ret void

[RootSignature("")]
min10float main( min10float mf:P ) : SV_Target
{
    return  mf * 2;
}