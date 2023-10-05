// RUN: dxc -Tlib_6_7 %s -verify

SamplerComparisonState s;
Texture1D t;

float foo(float a) {
  return t.CalculateLevelOfDetail(s, a) + // expected-error {{overload of intrinsic CalculateLevelOfDetail requires shader model 6.8 or greater}}
    t.CalculateLevelOfDetailUnclamped(s, a); // expected-error {{overload of intrinsic CalculateLevelOfDetailUnclamped requires shader model 6.8 or greater}}
}
