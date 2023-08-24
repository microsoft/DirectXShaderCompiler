// RUN: %dxc -T lib_6_8 %s | FileCheck %s -check-prefixes=CHECK,CHECKOPT
// RUN: %dxc -T lib_6_8 -Od %s | FileCheck %s -check-prefixes=CHECK,CHECKOD
// ==================================================================
// Test writing to matrix members of node records
// ==================================================================

struct RECORD
{
  row_major float2x2 m0;
  row_major float2x2 m1;
  column_major float2x2 m2;
};

// CHECK: %[[RECORD:struct\.RECORD.*]] = type { [4 x float], [4 x float], [4 x float] }

// CHECK-LABEL: define void @node01
[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Broadcasting")]
void node01(RWDispatchNodeInputRecord<RECORD> input1)
{
  // CHECKOPT: %[[p1_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 0
  // CHECKOD: %[[p1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1
  // CHECKOD: %[[p1_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 0
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_0]]
  // CHECKOPT: %[[p1_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 1
  // CHECKOD: %[[p1_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 1
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_1]]
  // CHECKOPT: %[[p1_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 2
  // CHECKOD: %[[p1_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 2
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_2]]
  // CHECKOPT: %[[p1_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 3
  // CHECKOD: %[[p1_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 3
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_3]]
  input1.Get().m1 = 111;
  // CHECKOD: %[[p2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2
  // CHECKOD: %[[p0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0
  // CHECKOPT: %[[p0_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 0
  // CHECKOD: %[[p0_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 0
  // CHECK: %[[v0_0:[^ ]+]] = load float, float addrspace(6)* %[[p0_0]]
  // CHECKOPT: %[[p0_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 1
  // CHECKOD: %[[p0_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 1
  // CHECK: %[[v0_1:[^ ]+]] = load float, float addrspace(6)* %[[p0_1]]
  // CHECKOPT: %[[p0_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 2
  // CHECKOD: %[[p0_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 2
  // CHECK: %[[v0_2:[^ ]+]] = load float, float addrspace(6)* %[[p0_2]]
  // CHECKOPT: %[[p0_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 3
  // CHECKOD: %[[p0_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 3
  // CHECK: %[[v0_3:[^ ]+]] = load float, float addrspace(6)* %[[p0_3]]
  // Note: store transposed.
  // CHECKOPT: %[[p2_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 0
  // CHECKOD: %[[p2_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 0
  // CHECK: store float %[[v0_0]], float addrspace(6)* %[[p2_0]]
  // CHECKOPT: %[[p2_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 1
  // CHECKOD: %[[p2_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 1
  // CHECK: store float %[[v0_2]], float addrspace(6)* %[[p2_1]]
  // CHECKOPT: %[[p2_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 2
  // CHECKOD: %[[p2_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 2
  // CHECK: store float %[[v0_1]], float addrspace(6)* %[[p2_2]]
  // CHECKOPT: %[[p2_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 3
  // CHECKOD: %[[p2_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 3
  // CHECK: store float %[[v0_3]], float addrspace(6)* %[[p2_3]]
  input1.Get().m2 = input1.Get().m0;
}

// CHECK-LABEL: define void @node02
[Shader("node")]
[NumThreads(1,1,1)]
[NodeLaunch("coalescing")]
void node02([MaxRecords(4)] RWGroupNodeInputRecords<RECORD> input2)
{
  // CHECKOPT: %[[p1_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 0
  // CHECKOD: %[[p1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1
  // CHECKOD: %[[p1_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 0
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_0]]
  // CHECKOPT: %[[p1_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 1
  // CHECKOD: %[[p1_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 1
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_1]]
  // CHECKOPT: %[[p1_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 2
  // CHECKOD: %[[p1_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 2
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_2]]
  // CHECKOPT: %[[p1_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 3
  // CHECKOD: %[[p1_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 3
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_3]]
  input2[0].m1 = 111;
  // CHECKOD: %[[p2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2
  // CHECKOD: %[[p0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0
  // CHECKOPT: %[[p0_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 0
  // CHECKOD: %[[p0_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 0
  // CHECK: %[[v0_0:[^ ]+]] = load float, float addrspace(6)* %[[p0_0]]
  // CHECKOPT: %[[p0_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 1
  // CHECKOD: %[[p0_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 1
  // CHECK: %[[v0_1:[^ ]+]] = load float, float addrspace(6)* %[[p0_1]]
  // CHECKOPT: %[[p0_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 2
  // CHECKOD: %[[p0_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 2
  // CHECK: %[[v0_2:[^ ]+]] = load float, float addrspace(6)* %[[p0_2]]
  // CHECKOPT: %[[p0_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 0, i32 3
  // CHECKOD: %[[p0_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p0]], i32 0, i32 3
  // CHECK: %[[v0_3:[^ ]+]] = load float, float addrspace(6)* %[[p0_3]]
  // Note: store transposed.
  // CHECKOPT: %[[p2_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 0
  // CHECKOD: %[[p2_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 0
  // CHECK: store float %[[v0_0]], float addrspace(6)* %[[p2_0]]
  // CHECKOPT: %[[p2_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 1
  // CHECKOD: %[[p2_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 1
  // CHECK: store float %[[v0_2]], float addrspace(6)* %[[p2_1]]
  // CHECKOPT: %[[p2_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 2
  // CHECKOD: %[[p2_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 2
  // CHECK: store float %[[v0_1]], float addrspace(6)* %[[p2_2]]
  // CHECKOPT: %[[p2_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 3
  // CHECKOD: %[[p2_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 3
  // CHECK: store float %[[v0_3]], float addrspace(6)* %[[p2_3]]
  input2[1].m2 = input2[1].m0;
}

// CHECK-LABEL: define void @node03
[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Broadcasting")]
void node03(NodeOutput<RECORD> output3)
{
  ThreadNodeOutputRecords<RECORD> outrec = output3.GetThreadNodeOutputRecords(1);
  // CHECKOPT: %[[p1_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 0
  // CHECKOD: %[[p1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1
  // CHECKOD: %[[p1_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 0
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_0]]
  // CHECKOPT: %[[p1_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 1
  // CHECKOD: %[[p1_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 1
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_1]]
  // CHECKOPT: %[[p1_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 2
  // CHECKOD: %[[p1_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 2
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_2]]
  // CHECKOPT: %[[p1_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 3
  // CHECKOD: %[[p1_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 3
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_3]]
  outrec.Get().m1 = 111;
  // CHECKOPT: %[[p2_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 0
  // CHECKOD: %[[p2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2
  // CHECKOD: %[[p2_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 0
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_0]]
  // CHECKOPT: %[[p2_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 1
  // CHECKOD: %[[p2_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 1
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_1]]
  // CHECKOPT: %[[p2_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 2
  // CHECKOD: %[[p2_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 2
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_2]]
  // CHECKOPT: %[[p2_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 3
  // CHECKOD: %[[p2_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 3
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_3]]
  outrec.Get().m2 = 222;
}

// CHECK-LABEL: define void @node04
[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("Coalescing")]
void node04([MaxOutputRecords(5)] NodeOutput<RECORD> outputs4)
{
  GroupNodeOutputRecords<RECORD> outrec = outputs4.GetGroupNodeOutputRecords(1);
  // CHECKOPT: %[[p1_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 0
  // CHECKOD: %[[p1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1
  // CHECKOD: %[[p1_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 0
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_0]]
  // CHECKOPT: %[[p1_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 1
  // CHECKOD: %[[p1_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 1
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_1]]
  // CHECKOPT: %[[p1_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 2
  // CHECKOD: %[[p1_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 2
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_2]]
  // CHECKOPT: %[[p1_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 1, i32 3
  // CHECKOD: %[[p1_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p1]], i32 0, i32 3
  // CHECK: store float 1.110000e+02, float addrspace(6)* %[[p1_3]]
  outrec.Get().m1 = 111;
  // CHECKOPT: %[[p2_0:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 0
  // CHECKOD: %[[p2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2
  // CHECKOD: %[[p2_0:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 0
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_0]]
  // CHECKOPT: %[[p2_1:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 1
  // CHECKOD: %[[p2_1:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 1
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_1]]
  // CHECKOPT: %[[p2_2:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 2
  // CHECKOD: %[[p2_2:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 2
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_2]]
  // CHECKOPT: %[[p2_3:[^ ]+]] = getelementptr %[[RECORD]], %[[RECORD]] addrspace(6)* %{{[^,]+}}, i32 0, i32 2, i32 3
  // CHECKOD: %[[p2_3:[^ ]+]] = getelementptr [4 x float], [4 x float] addrspace(6)* %[[p2]], i32 0, i32 3
  // CHECK: store float 2.220000e+02, float addrspace(6)* %[[p2_3]]
  outrec.Get().m2 = 222;
}
