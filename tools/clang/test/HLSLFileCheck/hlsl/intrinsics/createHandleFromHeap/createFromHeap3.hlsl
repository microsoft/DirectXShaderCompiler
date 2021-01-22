// RUN: %dxc -T ps_6_6 %s | %FileCheck %s

//CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 0, i1 false, i1 false)  ; CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
//CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<F32>
//CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 0, i1 true, i1 false)  ; CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
//CHECK:call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState


[RootSignature("RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED|SAMPLER_HEAP_DIRECTLY_INDEXED)")]
float4 main(float2 c:C) : SV_Target {

  Texture2D t = ResourceDescriptorHeap[0];
  SamplerState s = SamplerDescriptorHeap[0];
  return t.Sample(s, c);

}