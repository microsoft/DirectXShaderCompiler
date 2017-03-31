// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: FirstbitSHi

[RootSignature("")]
int main() : SV_Target {
    return firstbithigh((int)0xffffffff);
}