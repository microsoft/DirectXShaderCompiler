RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> y = ::ad::bwd::variable(context, 3.0f);
  output[0] = user::ad::bwd::f(context, 2.0f, y, 2.0f);
  output[1] = y.gradient(context);
}
