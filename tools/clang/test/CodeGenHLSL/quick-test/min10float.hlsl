// RUN: %dxc -E main -T ps_6_0 -Zi %s | FileCheck %s

// CHECK: warning: min12int is promoted to min16int
// CHECK: define void @main
// CHECK: ret void

[RootSignature("")]
min10float main( min10float mf:P ) : SV_Target
{
    return  mf * 2;
}