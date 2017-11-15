// RUN: %dxc -E main -T ps_6_2 -HV 2018 %s | FileCheck %s

// CHECK: int16_t is only supported with -enable-16bit-types option
// CHECK: uint16_t is only supported with -enable-16bit-types option
// CHECK: float16_t is only supported with -enable-16bit-types option

// int64_t/uint64_t already supported from 6.0

float4 main(float col : COL) : SV_TARGET
{
    int16_t i0;
    uint16_t i2;
    float16_t f0;
    return col;
}
