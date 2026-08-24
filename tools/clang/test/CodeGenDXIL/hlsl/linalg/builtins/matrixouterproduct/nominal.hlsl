// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -E main -fcgl %s | FileCheck %s --check-prefix=CHECK2

[numthreads(1,1,1)]
void main() {
  // CHECK-LABEL: define void @main()

  float4 lhs1 = {1,2,3,4};
  float4 rhs1 = {4,3,2,1};

  // CHECK: call %dx.types.LinAlgMatrixC9M4N4U2S0 @dx.op.linAlgMatrixOuterProduct.mC9M4N4U2S0.v4f32.v4f32
  // CHECK-SAME: (i32 -2147483619, <4 x float> {{.*}}, <4 x float> {{.*}})  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC9M4N4U2S0*, <4 x float>, <4 x float>)"
  // CHECK2: (i32 417, %dx.types.LinAlgMatrixC9M4N4U2S0* {{.*}}, <4 x float> {{.*}}, <4 x float> {{.*}})
  // Matrix<F32, 4, 4, Accumulator, Thread>
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(9, 4, 4, 2, 0)]] mat1;
  __builtin_LinAlg_MatrixOuterProduct(mat1, lhs1, rhs1);

  double4 lhs2 = {1,2,3,4};
  double4 rhs2 = {4,3,2,1};

  // CHECK: call %dx.types.LinAlgMatrixC10M4N4U2S0 @dx.op.linAlgMatrixOuterProduct.mC10M4N4U2S0.v4f64.v4f64
  // CHECK-SAME: (i32 -2147483619, <4 x double> {{.*}}, <4 x double> {{.*}})  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC10M4N4U2S0*, <4 x double>, <4 x double>)"
  // CHECK2: (i32 417, %dx.types.LinAlgMatrixC10M4N4U2S0* {{.*}}, <4 x double> {{.*}}, <4 x double> {{.*}})
  // Matrix<F64, 4, 4, Accumulator, Thread>
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(10, 4, 4, 2, 0)]] mat2;
  __builtin_LinAlg_MatrixOuterProduct(mat2, lhs2, rhs2);

  vector<int64_t, 4> lhs3 = {1,2,3,4};
  vector<int64_t, 4> rhs3 = {4,3,2,1};

  // CHECK: call %dx.types.LinAlgMatrixC6M4N4U2S0 @dx.op.linAlgMatrixOuterProduct.mC6M4N4U2S0.v4i64.v4i64
  // CHECK-SAME: (i32 -2147483619, <4 x i64> {{.*}}, <4 x i64> {{.*}})  ; LinAlgMatrixOuterProduct(vectorA,vectorB)

  // CHECK2: call void @"dx.hl.op..void (i32, %dx.types.LinAlgMatrixC6M4N4U2S0*, <4 x i64>, <4 x i64>)"
  // CHECK2: (i32 417, %dx.types.LinAlgMatrixC6M4N4U2S0* {{.*}}, <4 x i64> {{.*}}, <4 x i64> {{.*}})
  // Matrix<I64, 4, 4, Accumulator, Thread>
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(6, 4, 4, 2, 0)]] mat3;
  __builtin_LinAlg_MatrixOuterProduct(mat3, lhs3, rhs3);
}
