RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float2> context =
      (::ad::bwd::GradientContext<float2>)0;
  ::ad::bwd::Variable<float2> value =
      ::ad::bwd::variable(context, float2(2.0f, 3.0f));
  Result seed = {float2(7.0f, 11.0f), 13.0f};
  Result primal = user::ad::bwd::f(context, value, seed);
  float2 gradient = value.gradient(context);
  output[0] = primal.position.x;
  output[1] = primal.position.y;
  output[2] = primal.weight;
  output[3] = gradient.x;
  output[4] = gradient.y;
}
