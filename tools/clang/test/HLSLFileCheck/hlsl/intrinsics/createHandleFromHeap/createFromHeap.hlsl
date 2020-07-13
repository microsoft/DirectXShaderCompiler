// RUN: %dxc -T ps_6_6 %s | %FileCheck %s
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 216
// CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 217, %dx.types.Handle %{{.*}}, i8 0, i8 10, %dx.types.ResourceProperties { i32 489, i32 0 }) ; AnnotateHandle(res,resourceClass,resourceKind,props)  resource: TypedBuffer<F32>

uint ID;
float main(uint i:I): SV_Target {
  Buffer<float> buf = CreateResourceFromHeap(ID);
  return buf[i];
}