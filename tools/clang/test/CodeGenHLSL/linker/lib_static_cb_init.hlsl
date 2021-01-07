

float a;
static float b = 3;
float init(float t) {
  return t + (b++);
}
static float sa = init(a+1);

export float foo() {
  return sa;
}