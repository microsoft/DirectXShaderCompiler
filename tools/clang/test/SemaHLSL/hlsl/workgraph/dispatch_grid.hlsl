// RUN: %dxc -Tlib_6_8 -verify %s

struct Record1 {
// expected-error@+1{{SV_DispatchGrid should be 32/16 bit uint scalar or vector/array up to 3 elements}}
  uint4 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node1(DispatchNodeInputRecord<Record1> input) {}

struct Record2 {
// expected-error@+1{{SV_DispatchGrid should be 32/16 bit uint scalar or vector/array up to 3 elements}}
  uint grid[4] : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node2(DispatchNodeInputRecord<Record2> input) {}


struct Record3 {
// expected-error@+1{{SV_DispatchGrid should be 32/16 bit uint scalar or vector/array up to 3 elements}}
  uint2x2 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node3(DispatchNodeInputRecord<Record3> input) {}


struct Record4 {
// expected-error@+1{{SV_DispatchGrid should be 32/16 bit uint scalar or vector/array up to 3 elements}}
  int3 grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node4(DispatchNodeInputRecord<Record4> input) {}

struct Record5 {
// expected-error@+1{{SV_DispatchGrid should be 32/16 bit uint scalar or vector/array up to 3 elements}}
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
// expected-error@+1{{SV_DispatchGrid should be 32/16 bit uint scalar or vector/array up to 3 elements}}
  T grid : SV_DispatchGrid;
  float data;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(32, 16, 1)]
[NumThreads(32, 1, 1)]
void node3(DispatchNodeInputRecord<Record6> input) {}
