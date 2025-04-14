// RUN: %dxc -spirv -HV 202x -Od -T cs_6_9 %s | FileCheck %s

// Issue #7302: implicit object argument of Get() evaluates to rvalue

template<class T, class U>
[[vk::ext_instruction(/*spv::OpBitcast*/124)]]
T bitcast(U);

struct Content
{
  int a;
};

// CHECK: [[INT:%[_0-9A-Za-z]*]] = OpTypeInt 32 1
// CHECK-DAG: [[I1:%[_0-9A-Za-z]*]] = OpConstant [[INT]] 1
// CHECK-DAG: [[IO:%[_0-9A-Za-z]*]] = OpConstant [[INT]] 0
// CHECK: [[UINT:%[_0-9A-Za-z]*]] = OpTypeInt 32 0
// CHECK-DAG: [[UDEADBEEF:%[_0-9A-Za-z]*]] = OpConstant [[UINT]] 3735928559
// CHECK-DAG: [[U0:%[_0-9A-Za-z]*]] = OpConstant [[UINT]] 0
// CHECK: [[V2UINT:%[_0-9A-Za-z]*]] = OpTypeVector [[UINT]] 2
// CHECK: [[VECTOR:%[_0-9A-Za-z]*]] = OpConstantComposite [[V2UINT]] [[UDEADBEEF]] [[U0]]
// CHECK: [[CONTENT:%[_0-9A-Za-z]*]] = OpTypeStruct [[INT]]
// CHECK: [[PPCONTENT:%[_0-9A-Za-z]*]] = OpTypePointer PhysicalStorageBuffer [[CONTENT]]
// CHECK: [[PPINT:%[_0-9A-Za-z]*]] = OpTypePointer PhysicalStorageBuffer [[INT]]

[numthreads(1, 1, 1)]
void main()
{
  bitcast<vk::BufferPointer<Content> >(uint32_t2(0xdeadbeefu,0x0u)).Get().a = 1;
}

// CHECK: [[BITCAST:%[0-9]*]] = OpBitcast [[PPCONTENT]] [[VECTOR]]
// CHECK: [[PTR:%[0-9]*]] = OpAccessChain [[PPINT]] [[BITCAST]] [[IO]]
// CHECK: OpStore [[PTR]] [[I1]] Aligned 4

