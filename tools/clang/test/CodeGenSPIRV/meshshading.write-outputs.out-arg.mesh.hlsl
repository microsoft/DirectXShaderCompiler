// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefixes=CHECK,MAINEXT
// RUN: %dxc -T ms_6_6 -E main -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefixes=CHECK,MAINNV
// RUN: not %dxc -T ms_6_6 -E main -DINOUT -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_SCALAR -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_ARRAY -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_FLOAT1 -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: %dxc -T ms_6_6 -E main -DARRAY -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefixes=ARRAY,EXT
// RUN: %dxc -T ms_6_6 -E main -DARRAY -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefixes=ARRAY,NV
// RUN: %dxc -T ms_6_6 -E main -DSIDE_EFFECT -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=SIDE
// RUN: %dxc -T ms_6_6 -E main -DSIDE_EFFECT -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=SIDE
// RUN: %dxc -T ms_6_6 -E main -DQUALIFIED -fcgl -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=QUAL
// RUN: %dxc -T ms_6_6 -E main -DQUALIFIED -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=QUAL
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM_DIRECT -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM_DIRECT -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM_DIRECT -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_ELEM_DIRECT -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: %dxc -T ms_6_6 -E main -DNORMALIZE -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=NORM
// RUN: %dxc -T ms_6_6 -E main -DNORMALIZE -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=NORM
// RUN: not %dxc -T ms_6_6 -E main -DMAT_NESTED -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_NESTED -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_NESTED -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_NESTED -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_CALL_RESULT -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_CALL_RESULT -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_CALL_RESULT -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_CALL_RESULT -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_IN_COMPOSITE -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_IN_COMPOSITE -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_IN_COMPOSITE -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_IN_COMPOSITE -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_TERNARY -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_TERNARY -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_TERNARY -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_TERNARY -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_COMPOUND -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_COMPOUND -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_COMPOUND -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_COMPOUND -fcgl -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_CALL_RESULT -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_CALL_RESULT -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: %dxc -T ms_6_6 -E main -spirv %s | FileCheck %s --check-prefixes=CHECK,MAINNV
// RUN: %dxc -T ms_6_6 -E main -DSCALARIZE -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=SCALAR
// RUN: %dxc -T ms_6_6 -E main -DSCALARIZE -spirv -fspv-target-env=vulkan1.3 -fspv-extension=SPV_NV_mesh_shader %s | FileCheck %s --check-prefix=SCALAR
// RUN: %dxc -T ms_6_6 -E main -DSCALARIZE -spirv %s | FileCheck %s --check-prefix=SCALAR
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_QUAL_VERTICES -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_QUAL_VERTICES -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_QUAL_PRIMITIVES -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_QUAL_PRIMITIVES -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_QUAL_INDICES -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_QUAL_INDICES -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_FLOAT1_BARE -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_FLOAT1_BARE -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_MAT11 -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_MAT11 -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_ROW -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_ROW -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_INT1 -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_INT1 -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_BOOL1 -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DINOUT_BOOL1 -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=INOUT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_IDX -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_IDX -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_IDX -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_IDX -fcgl -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_VSWZ -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_VSWZ -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_VSWZ -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_VSWZ -fcgl -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_COMPOUND -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_COMPOUND -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_COMPOUND -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_COMPOUND -fcgl -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_ARG -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_ARG -fcgl -spirv -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_ARG -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: not %dxc -T ms_6_6 -E main -DMAT_SWZ_ARG -fcgl -spirv -fspv-target-env=vulkan1.1 %s 2>&1 | FileCheck %s --check-prefix=MAT
// RUN: %dxc -T ms_6_6 -E main -DMAT_ROW -spirv -fspv-target-env=vulkan1.3 %s | FileCheck %s --check-prefix=ROW
// RUN: %dxc -T ms_6_6 -E main -DMAT_ROW -spirv -fspv-target-env=vulkan1.1 %s | FileCheck %s --check-prefix=ROW

// Mesh shader outputs passed as 'out' arguments are written back to the
// Output variables after the call.

// CHECK-DAG: OpDecorate [[POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// CHECK-DAG: OpDecorate [[PRIMITIVE_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// MAINEXT-DAG: OpDecorate [[INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveTriangleIndicesEXT
// MAINNV-DAG: OpDecorate [[INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveIndicesNV

// CHECK: [[VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_1
// CHECK-NEXT: OpStore [[VALUE]] %float_9
// CHECK-NEXT: OpStore [[VALUE]] %float_9
// CHECK: [[Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_2 %uint_2
// CHECK-NEXT: OpStore [[Z]] %float_5
// CHECK: [[W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[POSITION]] %int_2 %uint_3
// CHECK-NEXT: OpStore [[W]] %float_6
// CHECK: [[ID:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[PRIMITIVE_ID]] %int_0
// CHECK-NEXT: OpStore [[ID]] %uint_8

// MAINEXT: [[TRIANGLE:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint [[INDICES]] %int_0
// MAINEXT-NEXT: OpStore [[TRIANGLE]]
// MAINNV: [[INDEX0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[INDICES]] %uint_0
// MAINNV-NEXT: OpStore [[INDEX0]] %uint_0
// MAINNV: [[INDEX1:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[INDICES]] %uint_1
// MAINNV-NEXT: OpStore [[INDEX1]] %uint_1
// MAINNV: [[INDEX2:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[INDICES]] %uint_2
// MAINNV-NEXT: OpStore [[INDEX2]] %uint_2
// CHECK: [[EXTRA:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_0{{$}}
// CHECK-NEXT: OpStore [[EXTRA]] %float_9
// CHECK: [[STATIC_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0
// CHECK-NEXT: OpStore [[STATIC_VALUE]] %float_10

// INOUT: error: output arrays of a mesh shader can not be read from
// INOUT-NOT: error:

// MAT: error: matrix element writes to mesh shader outputs are not supported
// MAT-NOT: error:

// SCALAR-DAG: OpDecorate [[SCALAR_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// SCALAR: [[SCALAR_EXTRA:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_0{{$}}
// SCALAR-NEXT: OpStore [[SCALAR_EXTRA]] %float_9
// SCALAR: [[S:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_S %int_0{{$}}
// SCALAR-NEXT: OpStore [[S]] %float_9
// SCALAR: [[C:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_C %int_1 %uint_1{{$}}
// SCALAR-NEXT: OpStore [[C]] %float_9
// SCALAR: [[I1:%[0-9]+]] = OpAccessChain %_ptr_Output_int %out_var_I1 %int_0{{$}}
// SCALAR-NEXT: OpStore [[I1]] %int_9
// SCALAR: [[B1:%[0-9]+]] = OpAccessChain %_ptr_Output_uint %out_var_B1 %int_0{{$}}
// SCALAR-NEXT: OpStore [[B1]] %uint_1
// SCALAR: OpStore [[SCALAR_EXTRA]] %float_9
// SCALAR: [[SCALAR_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[SCALAR_ID]] %int_0
// SCALAR-NEXT: OpStore [[SCALAR_ID0]] %uint_1
// SCALAR: [[EXTRA2:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_2{{$}}
// SCALAR-NEXT: OpStore [[EXTRA2]] %float_10

// ROW: [[ROW0:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_MAT %int_0 %uint_1 %uint_0
// ROW-NEXT: OpStore [[ROW0]] %float_9
// ROW: [[ROW1:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_MAT %int_0 %uint_1 %uint_1
// ROW-NEXT: OpStore [[ROW1]] %float_9

// NORM-DAG: OpDecorate [[NORM_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// NORM-DAG: OpDecorate [[NORM_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// NORM: [[NORM_Y:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[NORM_POSITION]] %int_0 %uint_1
// NORM-NEXT: OpStore [[NORM_Y]] %float_9
// NORM-NEXT: OpStore [[NORM_Y]] %float_9
// NORM: [[NORM_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0{{$}}
// NORM-NEXT: OpStore [[NORM_VALUE]] %float_9
// NORM: [[NORM_EXTRA:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_0{{$}}
// NORM-NEXT: OpStore [[NORM_EXTRA]] %float_9
// NORM: OpStore [[NORM_VALUE]] %float_9
// NORM: OpStore [[NORM_Y]] %float_9
// NORM: [[NORM_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[NORM_ID]] %int_0
// NORM-NEXT: OpStore [[NORM_ID0]] %uint_1

// QUAL: %src_main = OpFunction
// QUAL: OpFunctionCall %void %inner %vertices %primitives %triangles
// QUAL-NOT: OpStore
// QUAL: OpFunctionEnd
// QUAL: %inner = OpFunction
// QUAL: [[INNER_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_1
// QUAL-NEXT: OpStore [[INNER_VALUE]]

// ARRAY-DAG: OpDecorate [[ARRAY_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// ARRAY-DAG: OpDecorate [[ARRAY_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// EXT-DAG: OpDecorate [[ARRAY_INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveTriangleIndicesEXT
// NV-DAG: OpDecorate [[ARRAY_INDICES:%[0-9A-Za-z_]+]] BuiltIn PrimitiveIndicesNV
// ARRAY: [[POS0:%[0-9]+]] = OpAccessChain %_ptr_Output_v4float [[ARRAY_POSITION]] %int_0
// ARRAY-NEXT: OpStore [[POS0]]
// ARRAY-NOT: OpStore
// ARRAY: [[VALUE0:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0
// ARRAY-NEXT: OpStore [[VALUE0]] %float_10
// ARRAY-NOT: OpStore
// ARRAY: [[ARRAY_EXTRA0:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_0{{$}}
// ARRAY-NEXT: OpStore [[ARRAY_EXTRA0]] %float_13
// ARRAY-NOT: OpStore
// ARRAY: [[POS1:%[0-9]+]] = OpAccessChain %_ptr_Output_v4float [[ARRAY_POSITION]] %int_1
// ARRAY-NEXT: OpStore [[POS1]]
// ARRAY-NOT: OpStore
// ARRAY: [[VALUE1:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_1
// ARRAY-NEXT: OpStore [[VALUE1]] %float_11
// ARRAY-NOT: OpStore
// ARRAY: [[ARRAY_EXTRA1:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_1{{$}}
// ARRAY-NEXT: OpStore [[ARRAY_EXTRA1]] %float_14
// ARRAY-NOT: OpStore
// ARRAY: [[POS2:%[0-9]+]] = OpAccessChain %_ptr_Output_v4float [[ARRAY_POSITION]] %int_2
// ARRAY-NEXT: OpStore [[POS2]]
// ARRAY-NOT: OpStore
// ARRAY: [[VALUE2:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_2
// ARRAY-NEXT: OpStore [[VALUE2]] %float_12
// ARRAY-NOT: OpStore
// ARRAY: [[ARRAY_EXTRA2:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_EXTRA %int_2{{$}}
// ARRAY-NEXT: OpStore [[ARRAY_EXTRA2]] %float_15
// ARRAY-NOT: OpStore
// ARRAY: [[ARRAY_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_ID]] %int_0
// ARRAY-NEXT: OpStore [[ARRAY_ID0]] %uint_8
// ARRAY-NOT: OpStore
// EXT: [[ARRAY_TRIANGLE:%[0-9]+]] = OpAccessChain %_ptr_Output_v3uint [[ARRAY_INDICES]] %int_0
// EXT-NEXT: OpStore [[ARRAY_TRIANGLE]]
// NV: [[ARRAY_INDEX0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_INDICES]] %uint_0
// NV-NEXT: OpStore [[ARRAY_INDEX0]] %uint_0
// NV-NOT: OpStore
// NV: [[ARRAY_INDEX1:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_INDICES]] %uint_1
// NV-NEXT: OpStore [[ARRAY_INDEX1]] %uint_1
// NV-NOT: OpStore
// NV: [[ARRAY_INDEX2:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[ARRAY_INDICES]] %uint_2
// NV-NEXT: OpStore [[ARRAY_INDEX2]] %uint_2
// ARRAY-NOT: OpStore
// ARRAY: OpFunctionEnd

// SIDE-DAG: OpDecorate [[SIDE_POSITION:%[0-9A-Za-z_]+]] BuiltIn Position
// SIDE-DAG: OpDecorate [[SIDE_ID:%[0-9A-Za-z_]+]] BuiltIn PrimitiveId
// SIDE: [[SIDE_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_0
// SIDE-NEXT: OpStore [[SIDE_VALUE]] %float_9
// SIDE: [[SIDE_Z:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[SIDE_POSITION]] %int_1 %uint_2
// SIDE-NEXT: OpStore [[SIDE_Z]] %float_5
// SIDE: [[SIDE_W:%[0-9]+]] = OpAccessChain %_ptr_Output_float [[SIDE_POSITION]] %int_1 %uint_3
// SIDE-NEXT: OpStore [[SIDE_W]] %float_6
// SIDE: [[SIDE_ID0:%[0-9]+]] = OpAccessChain %_ptr_Output_uint [[SIDE_ID]] %int_0
// SIDE-NEXT: OpStore [[SIDE_ID0]] %uint_2
// SIDE: OpStore [[SIDE_VALUE]] %float_13
// SIDE: OpStore [[SIDE_W]] %float_7
// SIDE: OpStore [[SIDE_Z]] %float_8
// SIDE: OpStore [[SIDE_ID0]] %uint_12
// SIDE: [[CAST_VALUE:%[0-9]+]] = OpAccessChain %_ptr_Output_float %out_var_VALUE %int_2
// SIDE-NEXT: OpStore [[CAST_VALUE]] %float_8

struct Vertex {
  float4 position : SV_Position;
  float value : VALUE;
  float1 extra : EXTRA;
  static float M;
#if defined(MAT_ELEM) || defined(MAT_ELEM_DIRECT) || defined(MAT_NESTED) || \
    defined(MAT_CALL_RESULT) || defined(MAT_IN_COMPOSITE) || \
    defined(MAT_TERNARY) || defined(MAT_COMPOUND) || defined(MAT_SWZ_IDX) || \
    defined(MAT_SWZ_VSWZ) || defined(MAT_SWZ_COMPOUND) || \
    defined(MAT_SWZ_ARG) || defined(MAT_ROW)
  float2x2 m : MAT;
#endif
#if defined(SCALARIZE) || defined(INOUT_MAT11) || defined(INOUT_ROW) || \
    defined(INOUT_INT1) || defined(INOUT_BOOL1)
  float1x1 s : S;
  float2x1 c : C;
  int1 i1 : I1;
  bool1 b1 : B1;
#endif
};

float Vertex::M = 1;

struct Primitive {
  uint id : SV_PrimitiveID;
};

struct Writer {
  float value;
  void write(out float x, out float2 pair) {
    x = value;
    pair = float2(7, 8);
  }
};

void setFloat(out float x) { x = 9; }
void setFloat1(out float1 x) { x = 10; }
void setInt(out int x) { x = 9; }
void setBool(out bool x) { x = true; }
void setPair(out float2 x) { x = float2(5, 6); }
void setUint(out uint x) { x = 8; }
void setTriangle(out uint3 x) { x = uint3(0, 1, 2); }
void bump(inout float x) { x += 1; }
void bumpI(inout int x) { x += 1; }
void bumpB(inout bool x) { x = !x; }
float setRet(out float x) { x = 9; return 1; }
float bumpRet(inout float x) { x += 1; return x; }
void setVertices(out Vertex v[3]) {
  v[0].position = float4(0, 0, 0, 1);
  v[0].value = 10;
  v[0].extra = 13;
  v[1].position = float4(1, 0, 0, 1);
  v[1].value = 11;
  v[1].extra = 14;
  v[2].position = float4(0, 1, 0, 1);
  v[2].value = 12;
  v[2].extra = 15;
}
void setPrimitives(out Primitive p[1]) { p[0].id = 8; }
void setTriangles(out uint3 t[1]) { t[0] = uint3(0, 1, 2); }
void bumpArr(inout Vertex v[3]) { v[0].value += 1; }
void inner(out vertices Vertex v[3], out primitives Primitive p[1],
           out indices uint3 t[1]) {
  setFloat(v[1].value);
  setUint(p[0].id);
  setTriangle(t[0]);
}
void inoutVertices(inout vertices Vertex v[3]) { v[0].value = 9; }
void inoutPrimitives(inout primitives Primitive p[1]) { p[0].id = 8; }
void inoutIndices(inout indices uint3 t[1]) { t[0] = uint3(0, 1, 2); }

[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void main(out vertices Vertex vertices[3],
          out primitives Primitive primitives[1],
          out indices uint3 triangles[1], uint gi : SV_GroupIndex) {
  SetMeshOutputCounts(3, 1);
#if defined(MAT_ELEM)
  setFloat(vertices[0].m._m01);
#elif defined(MAT_ELEM_DIRECT)
  vertices[0].m._m01 = 9;
#elif defined(MAT_NESTED)
  vertices[0].value = (vertices[0].m._m01 = 9);
#elif defined(MAT_CALL_RESULT)
  vertices[1].value = setRet(vertices[0].m._m01);
#elif defined(MAT_IN_COMPOSITE)
  triangles[0] = uint3(0, 1, (vertices[0].m._m01 = 9));
#elif defined(MAT_TERNARY)
  vertices[1].value = gi > 0 ? (vertices[0].m._m01 = 9) : 1;
#elif defined(MAT_COMPOUND)
  vertices[1].value += (vertices[0].m._m01 = 9);
#elif defined(MAT_SWZ_IDX)
  vertices[0].m._m00_m11[0] = 9;
#elif defined(MAT_SWZ_VSWZ)
  vertices[0].m._m00_m11.y = 9;
#elif defined(MAT_SWZ_COMPOUND)
  vertices[0].m._m00_m11.y += 1;
#elif defined(MAT_SWZ_ARG)
  setFloat(vertices[0].m._m00_m11[0]);
#elif defined(MAT_ROW)
  vertices[0].m[1][0] = 9;
  vertices[0].m[1].y = 9;
#elif defined(INOUT_QUAL_VERTICES)
  inoutVertices(vertices);
#elif defined(INOUT_QUAL_PRIMITIVES)
  inoutPrimitives(primitives);
#elif defined(INOUT_QUAL_INDICES)
  inoutIndices(triangles);
#elif defined(INOUT_FLOAT1_BARE)
  bump(vertices[0].extra);
#elif defined(INOUT_MAT11)
  bump(vertices[0].s);
#elif defined(INOUT_ROW)
  bump(vertices[1].c[1]);
#elif defined(INOUT_INT1)
  bumpI(vertices[0].i1);
#elif defined(INOUT_BOOL1)
  bumpB(vertices[0].b1);
#elif defined(SCALARIZE)
  setFloat(vertices[0].extra);
  setFloat(vertices[0].s);
  setFloat(vertices[1].c[1]);
  setInt(vertices[0].i1);
  setBool(vertices[0].b1);
  int i = 0;
  setFloat(vertices[i++].extra);
  primitives[0].id = i;
  setFloat1(vertices[2].extra);
#elif defined(INOUT_CALL_RESULT)
  vertices[1].value = bumpRet(vertices[0].value);
#elif defined(NORMALIZE)
  setFloat(vertices[0].position.y.x);
  setFloat(vertices[0].position.y.x.x);
  setFloat(vertices[0].value.x.x);
  setFloat(vertices[0].extra.x.x);
  int i = 0;
  setFloat((vertices[i++]).value);
  setFloat(((vertices[0]).position).y);
  primitives[0].id = i;
#elif defined(QUALIFIED)
  inner(vertices, primitives, triangles);
#elif defined(ARRAY)
  setVertices(vertices);
  setPrimitives(primitives);
  setTriangles(triangles);
#elif defined(SIDE_EFFECT)
  int i = 0;
  setFloat(vertices[i++].value);
  setPair(vertices[i++].position.zw);
  primitives[0].id = i;
  Writer writer;
  writer.value = 13;
  int j = 0;
  writer.write(vertices[j++].value.x, vertices[j++].position.wz);
  primitives[0].id = j + 10;
  setUint(vertices[2].value);
#else
  setFloat(vertices[1].value);
  setFloat(vertices[1].value.x);
  setPair(vertices[2].position.zw);
  setUint(primitives[0].id);
  setTriangle(triangles[0]);
  setFloat(vertices[0].extra.x);
  setFloat(vertices[0].M);
  bump(vertices[0].M);
  vertices[0].value = Vertex::M;
#ifdef INOUT
  bump(vertices[1].value);
#endif
#ifdef INOUT_SCALAR
  bump(vertices[1].value.x);
#endif
#ifdef INOUT_ARRAY
  bumpArr(vertices);
#endif
#ifdef INOUT_FLOAT1
  bump(vertices[0].extra.x);
#endif
#endif
}
