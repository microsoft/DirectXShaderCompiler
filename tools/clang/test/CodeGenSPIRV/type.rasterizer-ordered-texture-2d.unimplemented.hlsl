// RUN: %dxc -T ps_6_6 -E main
RasterizerOrderedTexture2D<uint> rot;

static const struct {
  // CHECK: error: initializer for type 'RasterizerOrderedTexture2D<unsigned int>' unimplemented
	RasterizerOrderedTexture2D<uint> rot_field;
} cstruct = {rot};

void main() { }
