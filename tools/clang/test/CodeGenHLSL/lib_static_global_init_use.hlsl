
float update();

[shader("pixel")]
float test() : SV_Target {
  return update();
}