// RUN: %dxc -DTYPE=float -T hs_6_9 -verify %s

struct HsConstantData {
  float Edges[3] : SV_TessFactor;
  dx::HitObject hit;
};

struct LongVec {
  float4 f;
  dx::HitObject hit;
};

HsConstantData PatchConstantFunction( // expected-error{{HitObjects in patch constant function return type are not supported}}
				      dx::HitObject hit : V, // expected-error{{HitObjects in patch constant function parameters are not supported}}
				      LongVec lv : L) { // expected-error{{HitObjects in patch constant function parameters are not supported}}
  return (HsConstantData)0;
}

[domain("tri")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(32)]
[patchconstantfunc("PatchConstantFunction")]
void main() {
}
