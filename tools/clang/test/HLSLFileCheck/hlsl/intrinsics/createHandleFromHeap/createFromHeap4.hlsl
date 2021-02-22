// RUN: %dxc -T ps_6_6 %s | %FileCheck %s

//CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 0, i1 false, i1 false)  ; CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
//CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 13, i32 4 })  ; AnnotateHandle(res,props)  resource: CBuffer
//CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 1, i1 false, i1 false)  ; CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
//CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 15, i32 4 })  ; AnnotateHandle(res,props)  resource: TBuffer
//CHECK:call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %{{.*}}, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
//CHECK:call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %{{.*}}, i32 0, i32 undef)  ; BufferLoad(srv,index,wot)
struct A {
  float a;
};

static ConstantBuffer<A> C= ResourceDescriptorHeap[0];

float4 main(float2 c:C) : SV_Target {

TextureBuffer<A> T= ResourceDescriptorHeap[1];

  return C.a * T.a;

}