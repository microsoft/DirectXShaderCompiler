// RUN: %dxc -T ps_6_5 -E main %s -spirv -DCULL_TYPE=bool | FileCheck %s
// RUN: %dxc -T ps_6_5 -E main %s -spirv -DCULL_TYPE=uint | FileCheck %s
// RUN: %dxc -T ps_6_5 -E main %s -spirv -DCULL_TYPE=bool2 | FileCheck %s
// RUN: %dxc -T ps_6_5 -E main %s -spirv -DCULL_ARRAY | FileCheck %s

// RUN: %dxc -T ps_6_5 -E main %s -spirv -fcgl -DCULL_TYPE=bool | FileCheck %s --check-prefix=BOOL
// RUN: %dxc -T ps_6_5 -E main %s -spirv -fcgl -DCULL_TYPE=uint | FileCheck %s --check-prefix=UINT
// RUN: %dxc -T ps_6_5 -E main %s -spirv -fcgl -DCULL_TYPE=bool2 | FileCheck %s --check-prefix=BOOL2
// RUN: %dxc -T ps_6_5 -E main %s -spirv -fcgl -DCULL_ARRAY | FileCheck %s --check-prefix=ARRAY

// SV_CullPrimitive as a pixel shader input is replaced with 0, like DXIL.
// DXIL doesn't check the type here, so aggregates are replaced too.

// CHECK-NOT: CullPrimitiveEXT
// CHECK: OpEntryPoint Fragment %main "main" %out_var_SV_Target
// CHECK-NOT: CullPrimitiveEXT

// BOOL: [[null:%[0-9]+]] = OpConstantNull %bool
// BOOL: OpStore %param_var_cull [[null]]

// UINT: [[null:%[0-9]+]] = OpConstantNull %uint
// UINT: OpStore %param_var_cull [[null]]

// BOOL2: [[null:%[0-9]+]] = OpConstantNull %v2bool
// BOOL2: OpStore %param_var_cull [[null]]

// ARRAY: [[null:%[0-9]+]] = OpConstantNull %_arr_bool_uint_2
// ARRAY: OpStore %param_var_cull [[null]]

#ifdef CULL_ARRAY
float4 main(bool cull[2] : SV_CullPrimitive) : SV_Target { return cull[1]; }
#else
float4 main(CULL_TYPE cull : SV_CullPrimitive) : SV_Target { return cull.x; }
#endif
