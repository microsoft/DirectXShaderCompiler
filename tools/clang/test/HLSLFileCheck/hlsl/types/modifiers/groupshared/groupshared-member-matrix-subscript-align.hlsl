// RUN: %dxc -DTYPE=Data /Tcs_6_0 %s | FileCheck %s -check-prefixes=CHECK,LCHECK
// RUN: %dxc -DTYPE=float4x4 /Tcs_6_0 %s | FileCheck %s -check-prefixes=CHECK,LCHECK
// RUN: %dxc -DTYPE=float4x1 /Tcs_6_0 %s | FileCheck %s
// RUN: %dxc -DTYPE=float1x4 /Tcs_6_0 %s | FileCheck %s
// RUN: %dxc -DTYPE=float2x2 /Tcs_6_0 %s | FileCheck %s

struct Data {
   float4x4 m;
};

// Ensure that the global has an alignment
//CHECK: @"[[GV:\\01\?GData@@.*]]" = addrspace(3) global [[DIM:\[[0-9]+]] x float] undef, align 4

groupshared TYPE GData;
StructuredBuffer<TYPE> input : register(t0);
RWStructuredBuffer<TYPE> output : register(u0);

[numthreads(128,1,1)]
void main(uint Id : SV_DispatchThreadId, uint g : SV_GroupID)
{
  // Ensure that the stores have the proper alignments
  // CHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 0), align 4
  // CHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 1), align 4
  // CHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 2), align 4
  // CHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 3), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 4), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 5), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 6), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 7), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 8), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 9), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 10), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 11), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 12), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 13), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 14), align 4
  // LCHECK: store float %{{[0-9]*}}, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 15), align 4
  GData = input[0];
  GroupMemoryBarrierWithGroupSync();
  // Ensure that the loads have the proper alignments
  // CHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 0), align 4
  // CHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 1), align 4
  // CHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 2), align 4
  // CHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 3), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 4), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 5), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 6), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 7), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 8), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 9), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 10), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 11), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 12), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 13), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 14), align 4
  // LCHECK: load float, float addrspace(3)* getelementptr inbounds ([[DIM]] x float], [[DIM]] x float] addrspace(3)* @"[[GV]]", i32 0, i32 15), align 4
  output[Id] = GData;

}
