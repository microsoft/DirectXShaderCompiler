// RUN: %dxc -Tlib_6_8 -enable-16bit-types -verify %s

struct Record1 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'uint4'}}
  uint4 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node1(DispatchNodeInputRecord<Record1> input) {}

struct Record2 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'uint [4]'}}
  uint grid[4] : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node2(DispatchNodeInputRecord<Record2> input) {}


struct Record3 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'uint2x2'}}
  uint2x2 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node3(DispatchNodeInputRecord<Record3> input) {}


struct Record4 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int3'}}
  int3 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node4(DispatchNodeInputRecord<Record4> input) {}

struct Record5 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int [3]'}}
  int grid[3] : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node5(DispatchNodeInputRecord<Record5> input) {}

struct T {
  float a;
};
struct Record6 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'T'}}
  T grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node6(DispatchNodeInputRecord<Record6> input) {}

struct T2 {
  uint3 a;
};
struct Record7 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'T2'}}
  T2 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node7(DispatchNodeInputRecord<Record7> input) {}


struct Record8 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'float'}}
  float grid : SV_DispatchGrid;
  float data;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node8(DispatchNodeInputRecord<Record8> input) {}


struct Record9 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'float3'}}
  float3 grid : SV_DispatchGrid;
  float data;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node9(DispatchNodeInputRecord<Record9> input) {}

struct Record10 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'half'}}
  half grid : SV_DispatchGrid;
  float data;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node10(DispatchNodeInputRecord<Record10> input) {}


struct Record11 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'half3'}}
  half3 grid : SV_DispatchGrid;
  float data;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node11(DispatchNodeInputRecord<Record11> input) {}

struct Record12 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int16_t4'}}
  int16_t4 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node12(DispatchNodeInputRecord<Record12> input) {}

struct Record13 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int16_t [4]'}}
  int16_t grid[4] : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node13(DispatchNodeInputRecord<Record13> input) {}


struct Record14 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int16_t2x2'}}
  int16_t2x2 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node14(DispatchNodeInputRecord<Record14> input) {}


struct Record15 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int16_t3'}}
  int16_t3 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node15(DispatchNodeInputRecord<Record15> input) {}

struct Record16 {
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int16_t [3]'}}
  int16_t grid[3] : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node16(DispatchNodeInputRecord<Record16> input) {}

struct Record17 {
  uint16_t3 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node17(DispatchNodeInputRecord<Record17> input) {}

struct Record18 {
  uint16_t grid[3] : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node18(DispatchNodeInputRecord<Record18> input) {}

template<typename T>
struct OutputRecord {
// expected-error@+16{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'float'}}
// expected-error@+15{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<float, 3>'}}
// expected-error@+14{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int'}}
// expected-error@+13{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<int, 3>'}}
// expected-error@+12{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<unsigned int, 4>'}}
// expected-error@+11{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'unsigned int [4]'}}
// expected-error@+10{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'half'}}
// expected-error@+9{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<half, 3>'}}
// expected-error@+8{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'int16_t'}}
// expected-error@+7{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<int16_t, 3>'}}
// expected-error@+6{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<uint16_t, 4>'}}
// expected-error@+5{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'uint16_t [4]'}}
// expected-error@+4{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'vector<int16_t, 4>'}}
// expected-error@+3{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'matrix<int16_t, 2, 2>'}}
// expected-error@+2{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'S<int>'}}
// expected-error@+1{{SV_DispatchGrid can only be applied to a declaration of unsigned 32 or 16-bit integer scalar, vector, or array up to 3 elements, not 'S<float>'}}
  T grid : SV_DispatchGrid;
  float data;
};

#define OUTPUT_NODE_ENTRY(T,N) \
[Shader("node")] \
[NodeLaunch("broadcasting")] \
[NodeDispatchGrid(1,1,1)] \
[NumThreads(32, 1, 1)] \
void node_output_N(NodeOutput<OutputRecord< T > > output) {}


OUTPUT_NODE_ENTRY(float, 0);
OUTPUT_NODE_ENTRY(float3, 1);
OUTPUT_NODE_ENTRY(int, 2);
OUTPUT_NODE_ENTRY(int3, 3);
OUTPUT_NODE_ENTRY(uint4, 4);
OUTPUT_NODE_ENTRY(uint[4], 5);
OUTPUT_NODE_ENTRY(half, 6);
OUTPUT_NODE_ENTRY(half3, 7);
OUTPUT_NODE_ENTRY(int16_t, 8);
OUTPUT_NODE_ENTRY(int16_t3, 9);
OUTPUT_NODE_ENTRY(uint16_t4, 10);
OUTPUT_NODE_ENTRY(uint16_t[4], 11);

OUTPUT_NODE_ENTRY(int16_t4, 12);
OUTPUT_NODE_ENTRY(int16_t2x2, 13);
OUTPUT_NODE_ENTRY(uint16_t3 , 14);
OUTPUT_NODE_ENTRY(uint16_t[3] , 15);

template<typename T>
struct S {
  T a;
};
OUTPUT_NODE_ENTRY(S<int> , 16);
OUTPUT_NODE_ENTRY(S<float> , 17);
