RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> value = ::ad::bwd::variable(context, 1.0f);
  output[0] =
      user::ad::bwd::f(context, value, 2, 2.0f, 3.0f, 1.0f, 2.0f);
  output[1] = value.gradient(context);
}
