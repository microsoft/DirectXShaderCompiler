// RUN: %dxc -T lib_6_x %s | FileCheck %s

// Ensure 2d array is broken down into 2 1d arrays, properly initialized,
// modified, and passed to function calls

// CHECK-DAG: %[[h0:.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf0
// CHECK-DAG: %[[h1:.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf1
// CHECK-DAG: %[[h2:.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf2
// CHECK-DAG: %[[h3:.+]] = load %dx.types.Handle, %dx.types.Handle* @{{.*}}buf3
// CHECK: %[[a:.+]] = alloca [2 x %dx.types.Handle]
// CHECK-DAG: %[[gep0:.+]] = getelementptr inbounds [2 x %dx.types.Handle], [2 x %dx.types.Handle]* %[[a]], i32 0, i32 0
// CHECK: store %dx.types.Handle %[[h0]], %dx.types.Handle* %[[gep0]]
// CHECK-DAG: %[[gep1:.+]] = getelementptr inbounds [2 x %dx.types.Handle], [2 x %dx.types.Handle]* %[[a]], i32 0, i32 1
// CHECK: store %dx.types.Handle %[[h1]], %dx.types.Handle* %[[gep1]]

// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull %[[a]])

// CHECK: store %dx.types.Handle %[[h2]], %dx.types.Handle* %[[gep0]]
// CHECK: store %dx.types.Handle %[[h3]], %dx.types.Handle* %[[gep1]]

// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull %[[a]])

// CHECK: %[[CMP1:.+]] = icmp eq i32 %{{.+}}, 1
// CHECK: %[[h10_SEL:.+]] = select i1 %[[CMP1]], %dx.types.Handle %[[h2]], %dx.types.Handle %[[h1]]
// CHECK: store %dx.types.Handle %[[h10_SEL]], %dx.types.Handle* %[[gep1]], align 8
// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull %[[a]])

// CHECK: %[[CMP2:.+]] = icmp eq i32 %{{.+}}, 2
// CHECK: %[[h01_SEL:.+]] = select i1 %[[CMP2]], %dx.types.Handle %[[h10_SEL]], %dx.types.Handle %[[h2]]
// CHECK: store %dx.types.Handle %[[h01_SEL]], %dx.types.Handle* %[[gep0]], align 8
// CHECK: call void @{{.*}}useArray{{.*}}([2 x %dx.types.Handle]* nonnull %[[a]])

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
