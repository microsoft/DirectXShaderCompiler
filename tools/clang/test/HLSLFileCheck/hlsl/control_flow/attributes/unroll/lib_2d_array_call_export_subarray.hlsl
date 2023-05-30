// RUN: %dxc -T lib_6_x %s | FileCheck %s

// Ensure 2d array is broken down into 2 1d arrays, properly initialized,
// modified, and passed to function calls

// CHECK-DAG: [[h0:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf0
// CHECK-DAG: [[h1:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf1
// CHECK-DAG: [[h2:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf2
// CHECK-DAG: [[h3:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf3
// CHECK: [[tmp:%.+]] = alloca [2 x %dx.types.Handle]
// CHECK-DAG: [[gep0:%.+]] = getelementptr inbounds [2 x %dx.types.Handle], [2 x %dx.types.Handle]* [[tmp]], i32 0, i32 0
// CHECK-DAG: [[gep1:%.+]] = getelementptr inbounds [2 x %dx.types.Handle], [2 x %dx.types.Handle]* [[tmp]], i32 0, i32 1

// CHECK-DAG: store %dx.types.Handle [[h0]], %dx.types.Handle* [[gep0]]
// CHECK-DAG: store %dx.types.Handle [[h1]], %dx.types.Handle* [[gep1]]

// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull [[tmp]])

// CHECK-DAG: store %dx.types.Handle [[h2]], %dx.types.Handle* [[gep0]]
// CHECK-DAG: store %dx.types.Handle [[h3]], %dx.types.Handle* [[gep1]]

// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull [[tmp]])


// CHECK-DAG: [[cmp:%.*]] = icmp eq i32 {{%.*}}, 1
// CHECK-DAG: [[h4:%.*]] = select i1 [[cmp]], %dx.types.Handle [[h2]], %dx.types.Handle [[h1]]
// CHECK-DAG: store %dx.types.Handle [[h0]], %dx.types.Handle* [[gep0]]
// CHECK-DAG: store %dx.types.Handle [[h4]], %dx.types.Handle* [[gep1]]

// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull [[tmp]])


// CHECK-DAG: [[cmp2:%.*]] = icmp eq i32 {{%.*}}, 2
// CHECK-DAG: [[h5:%.*]] = select i1 [[cmp2]], %dx.types.Handle [[h4]], %dx.types.Handle [[h2]]
// CHECK-DAG: store %dx.types.Handle [[h5]], %dx.types.Handle* [[gep0]]
// CHECK-DAG: store %dx.types.Handle [[h3]], %dx.types.Handle* [[gep1]]


// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull [[tmp]])

void useArray(RWStructuredBuffer<float4> buffers[2]);

RWStructuredBuffer<float4> buf0;
RWStructuredBuffer<float4> buf1;
RWStructuredBuffer<float4> buf2;
RWStructuredBuffer<float4> buf3;
uint g_cond;

[shader("pixel")]
float main() : SV_Target {

  RWStructuredBuffer<float4> buffers[2][2] = { buf0, buf1, buf2, buf3, };

  [unroll]
  for (uint j = 0; j < 4; j++) {
    useArray(buffers[j%2]);
    if (g_cond == j) {
      buffers[j/2][j%2] = buffers[j%2][j/2];
    }
  }

  return 0;
}
