// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC1 -DLTV=uint16_t1 -DRTV=uint1 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC2 -DLTV=uint16_t2 -DRTV=uint2 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC3 -DLTV=uint16_t3 -DRTV=uint3 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC4 -DLTV=uint16_t4 -DRTV=uint4 %s | FileCheck %s

// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC1 -DLTV=uint1 -DRTV=uint16_t1 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC2 -DLTV=uint2 -DRTV=uint16_t2 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC3 -DLTV=uint3 -DRTV=uint16_t3 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC4 -DLTV=uint4 -DRTV=uint16_t4 %s | FileCheck %s

// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC1 -DLTV=bool1 -DRTV=int1 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC2 -DLTV=bool2 -DRTV=int2 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC3 -DLTV=bool3 -DRTV=int3 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC4 -DLTV=bool4 -DRTV=int4 %s | FileCheck %s

// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC1 -DLTV=float1 -DRTV=bool1 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC2 -DLTV=float2 -DRTV=bool2 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC3 -DLTV=float3 -DRTV=bool3 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC4 -DLTV=float4 -DRTV=bool4 %s | FileCheck %s

// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC1 -DLTV=float1 -DRTV=float16_t1 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC2 -DLTV=float2 -DRTV=float16_t2 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC3 -DLTV=float3 -DRTV=float16_t3 %s | FileCheck %s
// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types -DVEC4 -DLTV=float4 -DRTV=float16_t4 %s | FileCheck %s

// This file tests cast between two constexpr-vectors having same component count, but different component types.

// CHECK: define void @main()


RWByteAddressBuffer rwbab;

void main() : OUT {

#ifdef VEC1
 const LTV v = RTV(0);
 rwbab.Store<LTV>(256, v);
 
#elif VEC2 
 const LTV v = RTV(1, 2);
 rwbab.Store<LTV>(512, v);
 
#elif VEC3
 const LTV v = RTV(4, 5, 6);
 rwbab.Store<LTV>(1024, v);
 
#else
 const LTV v = RTV(7, 8, 9, 10);
 rwbab.Store<LTV>(2048, v);
#endif

}