// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// RUN: %dxc -T lib_6_9 %s -Fo %t.1
// RUN: %dxl -T ps_6_8 %t.1 | FileCheck %s --check-prefixes=CHECK,UNARY

// Tests non-native-vector behavior for vec ops that scalarize to something
//  more complex than a simple repetition of the same dx.op calls.

StructuredBuffer< vector<float, 4> > buf;
ByteAddressBuffer rbuf;

// CHECK-LABEL: define void @main()
[shader("pixel")]
float4 main(uint i : SV_PrimitiveID, uint4 m : M) : SV_Target {

  vector<float, 4> vec1 = rbuf.Load< vector<float, 4> >(i++*32);
  vector<float, 4> vec2 = rbuf.Load< vector<float, 4> >(i++*32);
  vector<float, 4> vec3 = rbuf.Load< vector<float, 4> >(i++*32);
  vector<bool, 4> bvec = rbuf.Load< vector<bool, 4> >(i++*32);
  vector<uint, 4> ivec1 = rbuf.Load< vector<uint, 4> >(i++*32);
  vector<uint, 4> ivec2 = rbuf.Load< vector<uint, 4> >(i++*32);
  vector<float, 4> res = 0;

  // CHECK: fdiv fast float
  // CHECK: fdiv fast float
  // CHECK: fdiv fast float
  // CHECK: fdiv fast float
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: call float @dx.op.unary.f32(i32 17, float %{{.*}}) ; Atan(value)
  // CHECK: fadd fast float %{{.*}}, 0x40
  // CHECK: fadd fast float %{{.*}}, 0x40
  // CHECK: fadd fast float %{{.*}}, 0x40
  // CHECK: fadd fast float %{{.*}}, 0x40
  // CHECK: fadd fast float %{{.*}}, 0xC0
  // CHECK: fadd fast float %{{.*}}, 0xC0
  // CHECK: fadd fast float %{{.*}}, 0xC0
  // CHECK: fadd fast float %{{.*}}, 0xC0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast oeq float %{{.*}}, 0
  // CHECK: fcmp fast oeq float %{{.*}}, 0
  // CHECK: fcmp fast oeq float %{{.*}}, 0
  // CHECK: fcmp fast oeq float %{{.*}}, 0
  // CHECK: fcmp fast oge float %{{.*}}, 0
  // CHECK: fcmp fast oge float %{{.*}}, 0
  // CHECK: fcmp fast oge float %{{.*}}, 0
  // CHECK: fcmp fast oge float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: fcmp fast olt float %{{.*}}, 0
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: and i1
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: select i1 %{{.*}}, float 0x
  // CHECK: select i1 %{{.*}}, float 0x
  res += atan2(vec1, vec2);

  // CHECK: fdiv fast float
  // CHECK: fdiv fast float
  // CHECK: fdiv fast float
  // CHECK: fdiv fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fcmp fast oge float
  // CHECK: fcmp fast oge float
  // CHECK: fcmp fast oge float
  // CHECK: fcmp fast oge float
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 6, float %{{.*}}) ; FAbs(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)
  // CHECK: call float @dx.op.unary.f32(i32 22, float %{{.*}}) ; Frc(value)

  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: select i1 %{{.*}}, float %{{.*}}, float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  res += fmod(vec1, vec3);

  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  res += ldexp(vec1, vec2);

  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: call float @dx.op.unary.f32(i32 23, float %{{.*}}) ; Log(value)
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: fmul fast float
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  // CHECK: call float @dx.op.unary.f32(i32 21, float %{{.*}}) ; Exp(value)
  res += pow(vec1, vec2);

  // CHECK: mul i32
  // CHECK: call i32 @dx.op.tertiary.i32(i32 49, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}) ; UMad(a,b,c)
  // CHECK: call i32 @dx.op.tertiary.i32(i32 49, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}) ; UMad(a,b,c)
  // CHECK: call i32 @dx.op.tertiary.i32(i32 49, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}) ; UMad(a,b,c)
  res += dot(ivec1, ivec2);

  // CHECK: call float  @dx.op.unary.f32(i32 29, float  %{{.*}}) ; Round_z(value)
  // CHECK: call float  @dx.op.unary.f32(i32 29, float  %{{.*}}) ; Round_z(value)
  // CHECK: call float  @dx.op.unary.f32(i32 29, float  %{{.*}}) ; Round_z(value)
  // CHECK: call float  @dx.op.unary.f32(i32 29, float  %{{.*}}) ; Round_z(value)
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  // CHECK: fsub fast float
  res *= modf(vec2, vec3);

  // CHECK: = or i1
  // CHECK: = or i1
  // CHECK: = or i1
  bvec ^= any(vec1);

  // CHECK: = and i1
  // CHECK: = and i1
  // CHECK: = and i1
  bvec ^= all(vec1);

  // CHECK: call {{.*}} @dx.op.wave
  // CHECK: call {{.*}} @dx.op.wave
  // CHECK: call {{.*}} @dx.op.wave
  // CHECK: call {{.*}} @dx.op.wave
  uint4 match = WaveMatch(bvec);

  return select(match, res, vec3);

}
