RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void testMain() {
  user::ad::fwd::Box forwardBox = (user::ad::fwd::Box)0;
  ::ad::fwd::Value<float> forward =
      forwardBox.square(::ad::fwd::Value<float>::Create(2.0f, 1.0f));
  output[0] = forward.value;
  output[1] = forward.derivative;

  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> x = ::ad::bwd::variable(context, 2.0f);
  output[2] = user::ad::bwd::math::composed(context, x, 3.0f);
  output[3] = x.gradient(context);

  user::ad::bwd::Box backwardBox = (user::ad::bwd::Box)0;
  ::ad::bwd::GradientContext<float> memberContext =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> y = ::ad::bwd::variable(memberContext, 3.0f);
  output[4] = backwardBox.square(memberContext, y, 2.0f);
  output[5] = y.gradient(memberContext);
}
