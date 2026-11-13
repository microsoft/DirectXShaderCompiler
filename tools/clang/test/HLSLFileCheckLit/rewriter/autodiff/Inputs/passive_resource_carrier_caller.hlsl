RWStructuredBuffer<float> output : register(u1);

[numthreads(1, 1, 1)]
void testMain() {
  ResourceCarrier carrier;
  carrier.texture = source;
  carrier.sink = gradientSink;

  ::ad::bwd::GradientContext<float> context =
      (::ad::bwd::GradientContext<float>)0;
  ::ad::bwd::Variable<float> value = ::ad::bwd::variable(context, 2.0f);
  user::ad::bwd::evaluate(context, carrier, value, 0, 3.0f);
  output[0] = value.gradient(context);
}
