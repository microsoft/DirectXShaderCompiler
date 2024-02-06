// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Check that stage flags are set correctly for different barrier modes with
// new the SM 6.8 barrier intrinsics.

// RDAT: FunctionTable[{{.*}}] = {

RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "node_barrier"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Node(15) << 16) + (SM 6.8 ((6 << 4) + 8)) = 0xF0068 = 983144
// RDAT: MinShaderTarget: 983144

[shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node_barrier() {
  GroupMemoryBarrierWithGroupSync();
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_device1"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// Compute(5), Library(6), Mesh(13), Amplification(14), Node(15) = 0xE060 = 57440
// RDAT: ShaderStageFlag: 57440
// MinShaderTarget: (Library(6) << 16) + (SM 6.8 ((6 << 4) + 8)) = 60068 = 393320
// RDAT: MinShaderTarget: 393320

[noinline] export
void fn_barrier_device1() {
  Barrier(UAV_MEMORY, DEVICE_SCOPE);
}

[noinline] export
void fn_barrier_device2() {
  Barrier(BAB, DEVICE_SCOPE);
}

[noinline] export
void fn_barrier_group1() {
  Barrier(GROUP_SHARED_MEMORY, GROUP_SYNC | GROUP_SCOPE);
}

[noinline] export
void fn_barrier_group2() {
  Barrier(BAB, GROUP_SYNC | GROUP_SCOPE);
}

[noinline] export
void fn_barrier_node1() {
  Barrier(NODE_INPUT_MEMORY, GROUP_SYNC | GROUP_SCOPE);
}

// RDAT-LABEL: UnmangledName: "node_barrier_device_in_call"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// Node(15) = 0x8000 = 32768
// RDAT: ShaderStageFlag: 32768
// MinShaderTarget: (Node(15) << 16) + (SM 6.8 ((6 << 4) + 8)) = 0xF0068 = 983144
// RDAT: MinShaderTarget: 983144

[shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node_barrier_device_in_call() {
  fn_barrier_device1();
  BAB.Store(0, 0);
}
