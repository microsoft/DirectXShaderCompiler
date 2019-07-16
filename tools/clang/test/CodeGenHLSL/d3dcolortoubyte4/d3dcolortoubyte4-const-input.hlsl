// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Test the following:
// 1) Input i to D3DCOLORtoUBYTE4() is swizzled to i.zyxw
// 2) Swizzled value is multiplied with constant 255

// CHECK: 765
// CHECK: 510
// CHECK: 255
// CHECK: 1020

int4 main (): OUT
{
    return D3DCOLORtoUBYTE4(float4(1, 2, 3, 4));
}

