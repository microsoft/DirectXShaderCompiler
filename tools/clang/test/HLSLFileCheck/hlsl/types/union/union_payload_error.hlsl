// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

union PayloadStruct {
  float Color;
};

PayloadStruct MulPayload(in PayloadStruct Payload:A, in float x) { // CHECK: error: union objects cannot have HLSL annotations applied to them
  Payload.Color *= x;
  return Payload;
}

void main(PayloadStruct p
          : Payload,
            float f
          : INPUT,
            out PayloadStruct o // CHECK: error: union objects cannot have HLSL annotations applied to them
          : SV_Target) {

  o = MulPayload(p, f);
  o.Color += p.Color;
}
