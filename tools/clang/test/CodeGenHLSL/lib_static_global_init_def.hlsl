cbuffer X {
  float f;
}

static float g[2] = { 1, f };

export
float update() {
  return g[1]++;
}