// RUN: %dxc -E main -T ps_6_2  %s | FileCheck %s

// CHECK: error: uint16_t is only supported with -enable-16bit-types option
// CHECK: error: int16_t is only supported with -enable-16bit-types option

float4 main(float col : COL) : SV_TARGET
{
    uint16_t x = 1;
    int16_t y = 2;
    return col;
}
