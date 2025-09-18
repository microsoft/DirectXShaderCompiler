// RUN: %dxc -T ps_6_9 %s | FileCheck %s

RWByteAddressBuffer buf;

float4 main(uint4 id: IN0) : SV_Target
{
  vector<float, 256> fVec = buf.Load<vector<float, 256> >(0);

  // CHECK: call <256 x float> @dx.op.quadReadLaneAt.v256f32(i32 122, <256 x float> %{{.*}}, i32 %{{.*}})  ; QuadReadLaneAt(value,quadLane)
  fVec = QuadReadLaneAt(fVec, id.x);

  // CHECK: call <256 x float> @dx.op.quadOp.v256f32(i32 123, <256 x float> %{{.*}}, i8 0)  ; QuadOp(value,op)
  fVec = QuadReadAcrossX(fVec);

  // CHECK: call <256 x float> @dx.op.quadOp.v256f32(i32 123, <256 x float> %{{.*}}, i8 1)  ; QuadOp(value,op)
  fVec = QuadReadAcrossY(fVec);

  // CHECK: call <256 x float> @dx.op.quadOp.v256f32(i32 123, <256 x float> %{{.*}}, i8 2)  ; QuadOp(value,op)
  fVec = QuadReadAcrossDiagonal(fVec);

  float4 ret = { fVec[id.x], fVec[id.y], fVec[id.z], fVec[id.w]};

  return ret;
}
