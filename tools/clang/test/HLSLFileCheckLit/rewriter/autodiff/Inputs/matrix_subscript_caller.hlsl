RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2x2> context =
      (::ad::bwd::GradientContext<float2x2>)0;
  ::ad::bwd::Variable<float2x2> value =
      ::ad::bwd::variable(context, float2x2(2.0f, 3.0f, 4.0f, 5.0f));
  output[0] = user::ad::bwd::f(context, value, 1, 0, 3.0f);
  float2x2 gradient = value.gradient(context);
  output[1] = gradient[0][0];
  output[2] = gradient[0][1];
  output[3] = gradient[1][0];
  output[4] = gradient[1][1];
}
