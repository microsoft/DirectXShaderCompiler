// RUN: %dxc -T lib_6_x %s | FileCheck %s

// Ensure 2d array is supported.

// Ensure BreakUpArrayAllocas doesn't misfire and ignore the function call
// users, causing everything but the calls to be moved to separate scalar
// arrays, then all that code, being dead, getting deleted, leaving just the
// passing of uninitialized arrays to the function calls.

// Because the array could be modified in the call, it needs to be
// re-initialized before each call. The tests below verify the initialization of
// the array between each call.

// CHECK-DAG: [[h0:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf0
// CHECK-DAG: [[h1:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf1
// CHECK-DAG: [[h2:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf2
// CHECK-DAG: [[h3:%.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf3
// CHECK: [[tmp:%.+]] = alloca [2 x [2 x %dx.types.Handle]]

// CHECK-DAG: [[gep0:%.+]] = getelementptr inbounds [2 x [2 x %dx.types.Handle]], [2 x [2 x %dx.types.Handle]]* [[tmp]], i32 0, i32 0, i32 0
// CHECK-DAG: [[gep1:%.+]] = getelementptr inbounds [2 x [2 x %dx.types.Handle]], [2 x [2 x %dx.types.Handle]]* [[tmp]], i32 0, i32 0, i32 1
// CHECK-DAG: [[gep2:%.+]] = getelementptr inbounds [2 x [2 x %dx.types.Handle]], [2 x [2 x %dx.types.Handle]]* [[tmp]], i32 0, i32 1, i32 0
// CHECK-DAG: [[gep3:%.+]] = getelementptr inbounds [2 x [2 x %dx.types.Handle]], [2 x [2 x %dx.types.Handle]]* [[tmp]], i32 0, i32 1, i32 1

// Store: {0, 1, 2, 3}
// CHECK-DAG: store %dx.types.Handle [[h0]], %dx.types.Handle* [[gep0]], align 8
// CHECK-DAG: store %dx.types.Handle [[h1]], %dx.types.Handle* [[gep1]], align 8
// CHECK-DAG: store %dx.types.Handle [[h2]], %dx.types.Handle* [[gep2]], align 8
// CHECK-DAG: store %dx.types.Handle [[h3]], %dx.types.Handle* [[gep3]], align 8

// CHECK: call void @{{.*}}useArray{{.*}}([2 x [2 x %dx.types.Handle]]* nonnull [[tmp]])

// Store: {0, 1, 2, 3}
// CHECK-DAG: store %dx.types.Handle [[h0]], %dx.types.Handle* [[gep0]], align 8
// CHECK-DAG: store %dx.types.Handle [[h1]], %dx.types.Handle* [[gep1]], align 8
// CHECK-DAG: store %dx.types.Handle [[h2]], %dx.types.Handle* [[gep2]], align 8
// CHECK-DAG: store %dx.types.Handle [[h3]], %dx.types.Handle* [[gep3]], align 8

// CHECK: call void @{{.*}}useArray{{.*}}([2 x [2 x %dx.types.Handle]]* nonnull [[tmp]])

// Store: {0, (1 | 2), 2, 3}

// CHECK-DAG: [[cmp:%.*]] = icmp eq i32 {{%.*}}, 1
// CHECK-DAG: [[h4:%.*]] = select i1 [[cmp]], %dx.types.Handle [[h2]], %dx.types.Handle [[h1]]

// CHECK-DAG: store %dx.types.Handle [[h0]], %dx.types.Handle* [[gep0]], align 8
// CHECK-DAG: store %dx.types.Handle [[h4]], %dx.types.Handle* [[gep1]], align 8
// CHECK-DAG: store %dx.types.Handle [[h2]], %dx.types.Handle* [[gep2]], align 8
// CHECK-DAG: store %dx.types.Handle [[h3]], %dx.types.Handle* [[gep3]], align 8

// CHECK: call void @{{.*}}useArray{{.*}}([2 x [2 x %dx.types.Handle]]* nonnull [[tmp]])

// Store: {0, (1 | 2), ((1 | 2) | 2), 3}

// CHECK-DAG: [[cmp2:%.*]] = icmp eq i32 {{%.*}}, 2
// CHECK-DAG: [[h5:%.*]] = select i1 [[cmp2]], %dx.types.Handle [[h4]], %dx.types.Handle [[h2]]

// CHECK-DAG: store %dx.types.Handle [[h0]], %dx.types.Handle* [[gep0]], align 8
// CHECK-DAG: store %dx.types.Handle [[h4]], %dx.types.Handle* [[gep1]], align 8
// CHECK-DAG: store %dx.types.Handle [[h5]], %dx.types.Handle* [[gep2]], align 8
// CHECK-DAG: store %dx.types.Handle [[h3]], %dx.types.Handle* [[gep3]], align 8

// CHECK: call void @{{.*}}useArray{{.*}}([2 x [2 x %dx.types.Handle]]* nonnull [[tmp]])
void useArray(RWStructuredBuffer<float4> buffers[2][2]);

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
    useArray(buffers);
    if (g_cond == j) {
      buffers[j/2][j%2] = buffers[j%2][j/2];
    }
  }

  return 0;
}
