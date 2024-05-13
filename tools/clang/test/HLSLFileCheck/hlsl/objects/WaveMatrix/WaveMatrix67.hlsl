// RUN: %dxc -E main -T cs_6_7 -select-validator internal %s | FileCheck %s 
// RUN: %dxc -E main -T cs_6_8 -select-validator internal %s | FileCheck %s

// CHECK-NOT: define void @main()

// CHECK: error: validation errors
// CHECK: Function: main: error: Opcode WaveMatrix_Annotate not valid in shader model cs_6_{{[7-8]}}
// CHECK: Function: main: error: Opcode WaveMatrix_Annotate not valid in shader model cs_6_{{[7-8]}}.

RWByteAddressBuffer rwbuf;

[NumThreads(64,1,1)]
void main(uint3 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex)
{
  WaveMatrixLeft<float, 16, 16> left;
  WaveMatrixRight<float, 16, 16> right;

// CHECK: WaveMatrix67.hlsl:21:18: error: Opcode WaveMatrix_Depth not valid in shader model cs_6_{{[7-8]}}.
// CHECK: WaveMatrix67.hlsl:22:18: error: Opcode WaveMatrix_Depth not valid in shader model cs_6_{{[7-8]}}.

  rwbuf.Store(0, left.MatrixDepth());
  rwbuf.Store(4, right.MatrixDepth());
}

// CHECK: Validation failed.
