union C {
  static uint f1;
  uint f2;
};

void main() {
  C c;
  c.f2 = 1;
  return;
}
