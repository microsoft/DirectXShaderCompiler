// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck %s

//CHECK:%[[FirstLane:[a-zA-Z0-9]+]] = call float @dx.op.waveReadLaneFirst.f32(i32 118,
// Make sure use FirstLane.
//CHECK:call float @dx.op.unary.f32(i32 13, float %[[FirstLane]])


float main(float i:I) : SV_Target {
  const float uniformIndex = WaveReadLaneFirst ( i ) ;
  if ( uniformIndex == i)
  {
    return sin(uniformIndex);
  } else {
    return 12;
  }

}