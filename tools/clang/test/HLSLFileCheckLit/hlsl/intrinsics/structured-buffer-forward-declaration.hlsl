// RUN: %dxc -T cs_6_0 -E main -HV 2021 %s | FileCheck %s
// RUN: %dxc -T cs_6_0 -E main -HV 202x %s | FileCheck %s

struct Element;
typedef StructuredBuffer<Element> InputBuffer;
typedef RWStructuredBuffer<Element> OutputBuffer;

struct Element {
  uint Value;
};

InputBuffer Input;
OutputBuffer Output;

// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call void @dx.op.bufferStore.i32
[numthreads(1, 1, 1)]
void main() {
  Output[0] = Input[0];
}
