// RUN: %dxc -E main -T lib_6_3 %s | FileCheck %s

//CHECK: User define type intrinsic arg must be struct

RayTracingAccelerationStructure Acc;

uint RayFlags;
uint InstanceInclusionMask;
uint RayContributionToHitGroupIndex;
uint MultiplierForGeometryContributionToHitGroupIndex;
uint MissShaderIndex;

RayDesc Ray;


float4 emit(inout float2 f2 )  {
  TraceRay(Acc,RayFlags,InstanceInclusionMask,
           RayContributionToHitGroupIndex,
           MultiplierForGeometryContributionToHitGroupIndex,MissShaderIndex , Ray, f2);

   return 2.6;
}