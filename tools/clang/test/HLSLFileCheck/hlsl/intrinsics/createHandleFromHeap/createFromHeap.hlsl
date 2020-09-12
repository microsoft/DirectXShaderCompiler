// RUN: %dxc -T ps_6_6 %s | %FileCheck %s
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{.*}}, i1 false, i1 false)  ; CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
// CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 265, i32 10 })  ; AnnotateHandle(res,props)  resource: TypedBuffer<F32>

uint ID;
float main(uint i:I): SV_Target {
  Buffer<float> buf = ResourceDescriptorHeap[ID];
  return buf[i];
}