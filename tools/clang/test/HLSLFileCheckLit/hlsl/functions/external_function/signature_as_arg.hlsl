// RUN: dxc -Tlib_6_3 %s | FileCheck %s

// Make sure function call works when argument is signature.
// Using lib profile to allow function has

// CHECK-LABEL: define void @main() {
// CHECK: %[[B:.+]] = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
// CHECK: %[[A_A:.+]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
// CHECK: %[[TmpDepth:.+]] = alloca float, align 4
// CHECK: %[[TmpOA:.+]] = alloca %struct.A, align 8
// CHECK: %[[TmpA:.+]] = alloca %struct.A, align 8
// CHECK: %[[WriteScalar:.+]] = call{{.*}} float @"\01?write_scalar@@YAMAIAM@Z"(float* nonnull dereferenceable(4) %[[TmpDepth]])
// CHECK: %[[TmpA_A:.+]] = getelementptr{{.*}} %struct.A, %struct.A* %[[TmpA]], i32 0, i32 0
// CHECK: store float %[[A_A]], float* %[[TmpA_A]], align 8
// CHECK: %[[ReadStruct:.+]] = call{{.*}} float @"\01?read_struct@@YAMUA@@@Z"(%struct.A* nonnull %[[TmpA]])
// CHECK: %[[ADD0:.+]] = fadd fast float %[[ReadStruct]], %[[WriteScalar]]
// CHECK: %[[WriteStruct:.+]] = call{{.*}} float @"\01?write_struct@@YAMUA@@@Z"(%struct.A* nonnull %[[TmpOA]])
// CHECK: %[[ADD1:.+]] = fadd fast float %[[ADD0]], %[[WriteStruct]]
// CHECK: %[[ReadScalar:.+]] = call{{.*}} float @"\01?read_scalar@@YAMM@Z"(float %[[B]])
// CHECK: %[[RET:.+]] = fadd fast float %[[ADD1]], %[[ReadScalar]]
// CHECK: %[[TmpOA_A:.+]] = getelementptr{{.*}} %struct.A, %struct.A* %[[TmpOA]], i32 0, i32 0
// CHECK: %[[OA_A:.+]] = load float, float* %[[TmpOA_A]], align 8
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %[[RET]])
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %[[OA_A]])
// CHECK: %[[Depth:.+]] = load float, float* %[[TmpDepth]], align 4
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %[[Depth]])
// CHECK: ret void

float write_scalar(out float depth);

float read_scalar(float a);


struct A {
  float a;
};

float read_struct(A a);

float write_struct(out A a);

[shader("pixel")]
float main(A a : A, float b : B, out A oa : SV_Target1, out float depth: SV_Depth) : SV_Target {
  return write_scalar(depth) + read_struct(a) + write_struct(oa) + read_scalar(b);
}
