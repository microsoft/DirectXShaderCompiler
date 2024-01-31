// RUN: %dxc -T lib_6_x %s | FileCheck %s
// XFAIL: *

struct RECORD
{
  half h;
};


DispatchNodeInputRecord<RECORD> wrapper(DispatchNodeInputRecord<RECORD> t);


RWBuffer<uint> buf0;


[Shader("node")]
[NumThreads(1024,1,1)]
[NodeDispatchGrid(64,1,1)]
[NodeLaunch("broadcasting")]
void node_DispatchNodeInputRecord(DispatchNodeInputRecord<RECORD> input)
{
  buf0[0] = wrapper(input).Get().h;
}


