// Run: %dxc -T ps_6_0 -E main


// CHECK: %type_StructuredBuffer_v2bool = OpTypeStruct %_runtimearr_v2uint
// CHECK: %type_RWStructuredBuffer_bool = OpTypeStruct %_runtimearr_uint

StructuredBuffer<bool2>  sb;
RWStructuredBuffer<bool> rwsb;

void main()
{
// CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %rwsb %int_0 %uint_0
// CHECK: [[uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK:                 OpStore %c [[bool]]
  bool  c = rwsb[0];

// CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %rwsb %int_0 %uint_1
// CHECK: [[uint:%\d+]] = OpSelect %uint %true %uint_1 %uint_0
// CHECK:                 OpStore [[ptr]] [[uint]]
  rwsb[1] = true;

// CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %rwsb %int_0 %uint_0
// CHECK: [[uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK:                 OpBranchConditional [[bool]] %if_true %if_merge
  if(rwsb[0]) {}

// CHECK:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %sb %int_0 %uint_1
// CHECK: [[uint2:%\d+]] = OpLoad %v2uint [[ptr]]
// CHECK: [[bool2:%\d+]] = OpINotEqual %v2bool [[uint2]] {{%\d+}}
// CHECK:                  OpStore %d [[bool2]]
  bool2 d = sb[1];

// CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %sb %int_0 %uint_1 %uint_0
// CHECK: [[uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK:                 OpBranchConditional [[bool]] %if_true_0 %if_merge_0
  if(sb[1][0]) {}
}
