RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> valueContext =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::GradientContext<float> scaleContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float2> value =
      ::ad::bwd::variable(valueContext, float2(2.0f, 3.0f));
  ::ad::bwd::Variable<float> scale =
      ::ad::bwd::variable(scaleContext, 4.0f);
  output[0] = user::ad::bwd::f(valueContext, scaleContext, 1.0f, value,
                                scale, 3.0f);
  float2 valueGradient = value.gradient(valueContext);
  output[1] = valueGradient.x;
  output[2] = valueGradient.y;
  output[3] = scale.gradient(scaleContext);
}
