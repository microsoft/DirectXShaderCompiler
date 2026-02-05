// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

RWByteAddressBuffer outbuf;

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U1S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U1S2 {{.*}}, %dx.types.Handle %{{.*}}, i32 1, i32 1, i32 0)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlg_MatrixStoreToDescriptor(mat1, outbuf, 1, 1, 0);
}
