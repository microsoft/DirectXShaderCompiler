union union {     /* error: declaration of anonymous union must be a definition */
  uint f1 ;
  uint f2 ;
};

[numthreads(1, 1, 1)]
void main() {
  union c;
  c.f1 = 1;
  return;
}

