// RUN: %dxc -T ps_6_5 -E main %s -spirv -DCULL_TYPE=bool | FileCheck %s
// RUN: %dxc -T ps_6_5 -E main %s -spirv -DCULL_TYPE=uint | FileCheck %s

// SV_CullPrimitive as a pixel shader input is replaced with 0, like DXIL.

// CHECK-NOT: CullPrimitiveEXT
// CHECK: OpEntryPoint Fragment %main "main" %out_var_SV_Target
// CHECK-NOT: CullPrimitiveEXT

float4 main(CULL_TYPE cull : SV_CullPrimitive) : SV_Target { return cull; }
