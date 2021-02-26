// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=uint -DRTV=uint4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=uint1 -DRTV=uint4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=uint2 -DRTV=uint4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=uint3 -DRTV=uint4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=uint4 -DRTV=uint4 %s | FileCheck %s

// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=float16_t -DRTV=float16_t4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=float16_t1 -DRTV=float16_t4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=float16_t2 -DRTV=float16_t4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=float16_t3 -DRTV=float16_t4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=float16_t4 -DRTV=float16_t4 %s | FileCheck %s

// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=bool -DRTV=float16_t4 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=bool1 -DRTV=float16_t4 %s | FileCheck %s
// #RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=bool2 -DRTV=float16_t4 %s | FileCheck %s
// #RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=bool3 -DRTV=float16_t4 %s | FileCheck %s
// #RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DLTV=bool4 -DRTV=float16_t4 %s | FileCheck %s

// This file tests truncation cast between two constexpr-vectors with different component count and/or types.

// CHECK: define void @main()


RWByteAddressBuffer rwbab;

void main() : OUT {

 // Case 1: all zero constant
 //const LTV v1 = RTV(0, 0, 0, 0);
 //rwbab.Store<LTV>(100, v1);
 
 // Case 2: Non-zero constant
 const LTV v2 = RTV(1, 2, 3, 4);
 rwbab.Store<LTV>(200, v2);

}