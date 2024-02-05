// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Ensure that barrier is allowed for node shaders.

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

// RDAT-LABEL: UnmangledName: "use_barrier"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// Compute(5), Library(6), Mesh(13), Amplification(14), Node(15) = 0xE060 = 57440
// RDAT: ShaderStageFlag: 57440
// MinShaderTarget: (Library(6) << 16) + (SM 6.0 ((6 << 4) + 0)) = 0x60060 = 393312
// RDAT: MinShaderTarget: 393312

[noinline] export
void use_barrier() {
  GroupMemoryBarrierWithGroupSync();
}

// RDAT-LABEL: UnmangledName: "node_barrier_in_call"
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
void node_barrier_in_call() {
  use_barrier();
  BAB.Store(0, 0);
}
