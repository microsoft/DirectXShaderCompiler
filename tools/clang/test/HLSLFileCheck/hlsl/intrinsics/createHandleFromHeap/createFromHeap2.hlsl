// RUN: %dxc -T ps_6_6 %s | %FileCheck %s

// Make sure snorm/unorm and globallycoherent works.
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 216
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 216
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 216
// CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 217, %dx.types.Handle %{{.*}}, i8 1, i8 10, %dx.types.ResourceProperties { i32 46, i32 0 })
// CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 217, %dx.types.Handle %{{.*}}, i8 1, i8 10, %dx.types.ResourceProperties { i32 45, i32 2 })
// CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 217, %dx.types.Handle %{{.*}}, i8 1, i8 10, %dx.types.ResourceProperties { i32 45, i32 2 })


struct S {
RWBuffer<unorm float> buf;
globallycoherent RWBuffer<snorm float> buf1[2];
};

uint ID;
float main(uint i:I): SV_Target {
  S s;
  s.buf = CreateResourceFromHeap(ID);
  s.buf1[0] = CreateResourceFromHeap(ID+1);
  s.buf1[1] = CreateResourceFromHeap(ID+2);
  return s.buf[i] + s.buf1[0][i] + s.buf1[1][i];
}