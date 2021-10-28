// RUN: %dxc -T cs_6_5 -E main -spirv

[[vk::ext_capability(/* RayQueryKHR */ 4472)]]
[[vk::ext_extension("SPV_KHR_ray_query")]]
[[vk::ext_type_def(/* Unique id for type */ 2,
                   /* OpTypeRayQueryKHR */ 4472)]]
void createTypeRayQueryKHR();

[[vk::ext_type_def(/* Unique id for type */ 3,
                   /* OpTypeAccelerationStructureKHR */ 5341)]]
void createAcceleStructureType();

[[vk::ext_instruction(/* OpRayQueryTerminateKHR */ 4474)]]
void rayQueryTerminateEXT(
       [[vk::ext_reference]] ext_type<2> rq);

ext_type<3> as : register(t0);

[[vk::ext_instruction(/* OpRayQueryInitializeKHR */  4473)]]
void rayQueryInitializeEXT([[vk::ext_reference]] ext_type<2>  rayQuery, ext_type<3> as, uint rayFlags, uint cullMask, float3 origin, float tMin, float3 direction, float tMax);

[[vk::ext_instruction(/* OpRayQueryTerminateKHR */ 4474)]]
void rayQueryTerminateEXT(
       [[vk::ext_reference]] ext_type<2> rq );

//CHECK: %spirvIntrinsicType = OpTypeAccelerationStructureKHR
//CHECK: %spirvIntrinsicType_0 = OpTypeRayQueryKHR

//CHECK: OpRayQueryInitializeKHR %rq {{%\w+}} {{%\w+}} {{%\w+}} {{%\w+}} {{%\w+}} {{%\w+}} {{%\w+}}
//CHECK: OpRayQueryTerminateKHR %rq

[numthreads(64, 1, 1)]
void main()
{
    createTypeRayQueryKHR();
    createAcceleStructureType();
    ext_type<2> rq;
    rayQueryInitializeEXT(rq, as, 0, 0, float3(0, 0, 0), 0.0, float3(1,1,1), 1.0);
    rayQueryTerminateEXT(rq);
}
