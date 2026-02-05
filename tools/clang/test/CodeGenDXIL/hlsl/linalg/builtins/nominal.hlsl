// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

ByteAddressBuffer inbuf;
RWByteAddressBuffer outbuf;

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC4M5N4U1S2 @dx.op.linAlgFillMatrix.mC4M5N4U1S2.i32(i32 -2147483636, i32 {{.*}})  ; LinAlgFillMatrix(value)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlg_FillMatrix(mat1, 5);
  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC5M3N4U0S0 @dx.op.linAlgFillMatrix.mC5M3N4U0S0.f32(i32 -2147483636, float {{.*}})  ; LinAlgFillMatrix(value)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(5, 3, 4, 0, 0)]] mat2;
  __builtin_LinAlg_FillMatrix(mat2, 3.14);

  // CHECK: call void @dx.op.linAlgMatrixStoreToDescriptor.mC4M5N4U1S2(i32 -2147483628, %dx.types.LinAlgMatrixC4M5N4U1S2 %{{.*}}, %dx.types.Handle %{{.*}}, i32 1, i32 1, i32 0)  ; LinAlgMatrixStoreToDescriptor(matrix,handle,offset,stride,layout)
  __builtin_LinAlg_MatrixStoreToDescriptor(mat1, outbuf, 1, 1, 0);

  // CHECK: %{{.*}} = call %dx.types.LinAlgMatrixC1M1N1U0S0 @dx.op.linAlgMatrixLoadFromDescriptor.mC1M1N1U0S0(i32 -2147483634, %dx.types.Handle %{{.*}}, i32 0, i32 0, i32 0)  ; LinAlgMatrixLoadFromDescriptor(handle,offset,stride,layout)
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 1, 0, 0)]] mat3;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat3, inbuf, 0, 0, 0);
}
