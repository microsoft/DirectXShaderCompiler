// Run: %dxc -E main -T cs_6_0

RWStructuredBuffer<int> testrwbuf : register(u0);

void testfn(uint index, uint value, inout RWStructuredBuffer<int> buf);

[numthreads(1, 1, 1)]
void main(uint3 GroupId          : SV_GroupID,
          uint3 DispatchThreadId : SV_DispatchThreadID)
{
// CHECK: %param_var_buf = OpVariable %_ptr_Function__ptr_Uniform_type_RWStructuredBuffer_int Function
// CHECK:       {{%\d+}} = OpFunctionCall %void %testfn %param_var_index %param_var_value %param_var_buf
  testfn(GroupId.x, DispatchThreadId.x, testrwbuf);
}

// CHECK:   %buf = OpFunctionParameter %_ptr_Function__ptr_Uniform_type_RWStructuredBuffer_int
void testfn(uint index, uint value, inout RWStructuredBuffer<int> buf) {
  buf[index] = value;
}


