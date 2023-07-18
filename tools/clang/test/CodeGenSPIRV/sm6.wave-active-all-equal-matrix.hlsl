// RUN: %dxc -T cs_6_0 -HV 2018 -E main -fspv-target-env=vulkan1.1

struct S {
    float2x2 val;
    bool res;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformVote

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {

// Each element of the matrix must be extracted, and be passed to OpGroupNonUniformAllEqual. 
// CHECK: [[ld:%\w+]] = OpLoad %mat2v2float %34

// Process the first row.
// CHECK: [[row_0:%\w+]] = OpCompositeExtract %v2float [[ld]] 0
// CHECK: [[element_0_0:%\w+]] = OpCompositeExtract %float [[row_0]] 0
// CHECK: [[res_0_0:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element_0_0]]
// CHECK: [[element_0_1:%\w+]] = OpCompositeExtract %float [[row_0]] 1
// CHECK: [[res_0_1:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element_0_1]]

// Combine the results in a row for the results matrix.
// CHECK: [[res_0:%\w+]] = OpCompositeConstruct %v2bool [[res_0_0]] [[res_0_1]]

// Process the second row.
// CHECK: [[row_1:%\w+]] = OpCompositeExtract %v2float [[ld]] 1
// CHECK: [[element_1_0:%\w+]] = OpCompositeExtract %float [[row_1]] 0
// CHECK: [[res_1_0:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element_1_0]]
// CHECK: [[element_1_1:%\w+]] = OpCompositeExtract %float [[row_1]] 1
// CHECK: [[res_1_1:%\w+]] = OpGroupNonUniformAllEqual %bool %uint_3 [[element_1_1]]


// Combine the results in a row for the results matrix.
// CHECK: [[res_1:%\w+]] = OpCompositeConstruct %v2bool [[res_1_0]] [[res_1_1]]

// Combind the results for each row in a "matrix" for the final result.
// CHECK: [[res_matrix:%\w+]] = OpCompositeConstruct %_arr_v2bool_uint_2 [[res_0]] [[res_1]]

// Apply the `all` to the entire matrix.
// CHECK: [[res_vec0:%\w+]] = OpCompositeExtract %v2bool [[res_matrix]] 0
// CHECK: [[all0:%\w+]] = OpAll %bool [[res_vec0]]
// CHECK: [[res_vec1:%\w+]] = OpCompositeExtract %v2bool %53 1
// CHECK: [[all1:%\w+]] = OpAll %bool [[res_vec1]]
// CHECK: [[all_vec:%\w+]] = OpCompositeConstruct %v2bool [[all0]] [[all1]]
// CHECK: [[all:%\w+]] = OpAll %bool [[all_vec]]

// Do the select.
// CHECK: OpSelect %uint [[all]] %uint_1 %uint_0
    values[id.x].res = all(WaveActiveAllEqual(values[id.x].val));
}
