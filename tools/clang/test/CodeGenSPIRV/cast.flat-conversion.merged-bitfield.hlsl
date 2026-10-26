// RUN: %dxc -T ps_6_0 -E main -HV 2021 -fcgl %s -spirv | FileCheck %s

// Casting between structs whose bitfields merge into fewer SPIR-V members must
// extract by SPIR-V member index, not by AST field index.

struct Source {
  uint a : 16;
  uint b : 16;
  uint c;
};

struct Dest {
  uint a : 16;
  uint b : 16;
  uint c;
};

struct Base {
  uint base;
};

struct DerivedSource : Base {
  uint a : 16;
  uint b : 16;
  uint c;
};

struct DerivedDest : Base {
  uint a : 16;
  uint b : 16;
  uint c;
};

uint main(uint input : A) : SV_Target {
  Source src;
  src.a = 1;
  src.b = 2;
  src.c = input;

  DerivedSource dsrc;
  dsrc.base = input;
  dsrc.a = 3;
  dsrc.b = 4;
  dsrc.c = input;

// CHECK: [[s:%[0-9]+]] = OpLoad %Source %src
// CHECK: [[w:%[0-9]+]] = OpCompositeExtract %uint [[s]] 0
// CHECK: [[a:%[0-9]+]] = OpBitFieldUExtract %uint [[w]] %uint_0 %uint_16
// CHECK: [[b:%[0-9]+]] = OpBitFieldUExtract %uint [[w]] %uint_16 %uint_16
// CHECK: [[c:%[0-9]+]] = OpCompositeExtract %uint [[s]] 1
// CHECK: [[i:%[0-9]+]] = OpBitFieldInsert %uint [[a]] [[b]] %uint_16 %uint_16
// CHECK:                 OpCompositeConstruct %Dest [[i]] [[c]]
  Dest dst = (Dest)src;

// The base is member 0, so the packed word moves to member 1 and the plain
// member to member 2.
// CHECK: [[ds:%[0-9]+]] = OpLoad %DerivedSource %dsrc
// CHECK: [[base:%[0-9]+]] = OpCompositeExtract %Base [[ds]] 0
// CHECK: [[dw:%[0-9]+]] = OpCompositeExtract %uint [[ds]] 1
// CHECK: [[da:%[0-9]+]] = OpBitFieldUExtract %uint [[dw]] %uint_0 %uint_16
// CHECK: [[db:%[0-9]+]] = OpBitFieldUExtract %uint [[dw]] %uint_16 %uint_16
// CHECK: [[dc:%[0-9]+]] = OpCompositeExtract %uint [[ds]] 2
// CHECK: [[di:%[0-9]+]] = OpBitFieldInsert %uint [[da]] [[db]] %uint_16 %uint_16
// CHECK:                  OpCompositeConstruct %DerivedDest [[base]] [[di]] [[dc]]
  DerivedDest ddst = (DerivedDest)dsrc;

  return dst.c + ddst.base + ddst.c;
}
