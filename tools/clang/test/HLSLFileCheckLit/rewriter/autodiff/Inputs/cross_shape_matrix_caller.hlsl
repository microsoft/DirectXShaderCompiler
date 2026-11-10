RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> value =
      ::ad::bwd::variable(context, float2(2.0f, 3.0f));
  float2x2 primal = user::ad::bwd::f(
      context, value, float2x2(1.0f, 2.0f, 3.0f, 4.0f));
  float2 gradient = value.gradient(context);
  output[0] = primal[0][0];
  output[1] = primal[0][1];
  output[2] = primal[1][0];
  output[3] = primal[1][1];
  output[4] = gradient.x;
  output[5] = gradient.y;
}
