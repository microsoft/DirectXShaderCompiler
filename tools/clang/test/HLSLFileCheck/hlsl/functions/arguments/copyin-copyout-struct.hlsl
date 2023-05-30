// RUN: %dxc -T lib_6_4 -fcgl %s | FileCheck %s


struct Pup {
  float X;
};

void CalledFunction(inout float F, inout Pup P) {
  F = 4.0;
  P.X = 5.0;
}

void fn() {
  float X;
  Pup P;

  CalledFunction(X, P);
  CalledFunction(P.X, P);
}

// CHECK: define internal void @"\01?fn@
// CHECK-DAG: [[P:%[0-9A-Z]+]] = alloca %struct.Pup
// CHECK-DAG: [[X:%[0-9A-Z]+]] = alloca float, align 4
// CHECK-DAG: [[TmpP:%[0-9a-z.]+]] = alloca %struct.Pup

// CHECK: call void @"\01?CalledFunction{{[@$?.A-Za-z0-9_]+}}"(float* dereferenceable(4) [[X]], %struct.Pup*  dereferenceable(4) [[P]])

// CHECK-DAG: [[TmpPPtr:%[0-9]+]] = bitcast %struct.Pup* [[TmpP]] to i8*
// CHECK-DAG: [[PPtr:%[0-9]+]] = bitcast %struct.Pup* [[P]] to i8*
// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* [[TmpPPtr]], i8* [[PPtr]], i64 4, i32 1, i1 false)
// CHECK: [[PXPtr:%[0-9A-Z]+]] = getelementptr inbounds %struct.Pup, %struct.Pup* [[P]], i32 0, i32 0

// CHECK: call void @"\01?CalledFunction{{[@$?.A-Za-z0-9_]+}}"
// CHECK-SAME: (float* dereferenceable(4) [[PXPtr]], %struct.Pup*  dereferenceable(4) [[TmpP]])

// CHECK-DAG: [[PPtr:%[0-9]+]] = bitcast %struct.Pup* [[P]] to i8*
// CHECK-DAG: [[TmpPPtr:%[0-9]+]] = bitcast %struct.Pup* [[TmpP]] to i8*
// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* [[PPtr]], i8* [[TmpPPtr]], i64 4, i32 1, i1 false)

