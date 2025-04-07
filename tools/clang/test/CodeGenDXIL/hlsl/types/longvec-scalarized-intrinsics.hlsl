// RUN: %dxc -T ps_6_9 %s | FileCheck %s

// Long vector tests for vec ops that scalarize to something more complex
//  than a simple repetition of the same dx.op calls.

StructuredBuffer< vector<float, 8> > buf;
ByteAddressBuffer rbuf;

float4 main(uint i : SV_PrimitiveID, bool b : B) : SV_Target {
  vector<float, 8> vec1 = rbuf.Load< vector<float, 8> >(i++*32);
  vector<float, 8> vec2 = rbuf.Load< vector<float, 8> >(i++*32);
  vector<float, 8> vec3 = rbuf.Load< vector<float, 8> >(i++*32);

  // CHECK: fdiv fast <8 x float>
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: fadd fast <8 x float> %{{.*}}, <float 0x
  // CHECK: fadd fast <8 x float> %{{.*}}, <float 0x
  // CHECK: fcmp fast olt <8 x float>
  // CHECK: fcmp fast oeq <8 x float>
  // CHECK: fcmp fast oge <8 x float>
  // CHECK: fcmp fast olt <8 x float>
  // CHECK: and <8 x i1>
  // CHECK: select <8 x i1> %{{.*}}, <8 x float> %{{.*}}, <8 x float>
  // CHECK: and <8 x i1>
  // CHECK: select <8 x i1> %{{.*}}, <8 x float> %{{.*}}, <8 x float>
  // CHECK: and <8 x i1>
  // CHECK: select <8 x i1> %{{.*}}, <8 x float> <float 0x
  // CHECK: and <8 x i1>
  // CHECK: select <8 x i1> %{{.*}}, <8 x float> <float 0x
  vec1 = atan2(vec1, vec2);


  // CHECK: fdiv fast <8 x float>
  // CHECK: fsub fast <8 x float> <float
  // CHECK: fcmp fast oge <8 x float>
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)

  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)

  // CHECK: fsub fast <8 x float> <float
  // CHECK: select <8 x i1> %{{.*}}, <8 x float> %{{.*}}, <8 x float>
  // CHECK: fmul fast <8 x float>
  vec1 = fmod(vec1, vec2);

  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: fmul fast <8 x float>
  vec1 = ldexp(vec1, vec2);

  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: fmul fast <8 x float>
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  vec1 = pow(vec1, vec2);

  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: call float @dx.op.unary.f32(i32 29, float %{{.*}}) ; Round_z(value)
  // CHECK: fsub fast <8 x float>
  vec1 = modf(vec1, vec2);

  // CHECK: [[el:%.*]] = extractelement <8 x float>
  // CHECK: [[mul:%.*]] = fmul fast float [[el]]
  // CHECK: [[ping:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[mul]]) ; FMad(a,b,c)
  // CHECK: [[pong:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[ping]]) ; FMad(a,b,c)
  // CHECK: [[ping:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[pong]]) ; FMad(a,b,c)
  // CHECK: [[pong:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[ping]]) ; FMad(a,b,c)
  // CHECK: [[ping:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[pong]]) ; FMad(a,b,c)
  // CHECK: [[pong:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[ping]]) ; FMad(a,b,c)
  // CHECK: [[ping:%.*]] = call float @dx.op.tertiary.f32(i32 46, float %{{.*}}, float %{{.*}}, float [[pong]]) ; FMad(a,b,c)
  vec1 = dot(vec1, vec2);

  vector<bool, 8> bvec = b;
  // CHECK: or i1
  // CHECK: or i1
  // CHECK: or i1
  // CHECK: or i1
  // CHECK: or i1
  // CHECK: or i1
  // CHECK: or i1
  bvec &= any(vec1);

  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  bvec &= all(vec2);

  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  // call {{.*}} @dx.op.wave
  return WaveMatch(bvec);
}
