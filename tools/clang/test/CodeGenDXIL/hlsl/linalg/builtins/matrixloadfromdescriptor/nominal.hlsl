// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

ByteAddressBuffer inbuf;

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC1M1N1U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC1M1N1U0S0(i32 -2147483634, %dx.types.Handle %{{.*}}, i32 0, i32 0, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 1, 0, 0)]] mat;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, inbuf, 0, 0, 0);
}
