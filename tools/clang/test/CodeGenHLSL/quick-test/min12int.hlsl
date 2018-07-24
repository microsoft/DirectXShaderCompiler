// RUN: %dxc -E main -T ps_6_0 -Zi %s | FileCheck %s

// CHECK: warning: min12int is promoted to min16int
// CHECK: define void @main
// CHECK: ret void

[RootSignature("")]
min12int main( min12int mi:P ) : SV_Target
{
    return  mi * 2;
}