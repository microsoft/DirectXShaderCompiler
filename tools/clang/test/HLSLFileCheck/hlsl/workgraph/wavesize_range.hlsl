// RUN: %dxc -T lib_6_8 %s | FileCheck %s

// Check the WaveSize attribute is accepted by work graph nodes
// and appears in the metadata

struct INPUT_RECORD
{
  uint DispatchGrid1 : SV_DispatchGrid;
  uint2 a;
};



// CHECK: error: Preferred WaveSize value 32 must be between 4 and 16
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(4, 16, 32)]
void node01(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: Preferred WaveSize value 32 must be between 16 and 16
// CHECK: error: Minimum WaveSize value 16 must be less than Maximum WaveSize value 16
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(16, 16, 32)]
void node02(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: Minimum WaveSize value 16 must be less than Maximum WaveSize value 16
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(16, 16, 16)]
void node03(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// the non-power of 2 diagnostic gets emitted once, regardless of how many arguments aren't powers of 2.
// CHECK: WaveSize arguments must be between 4 and 128 and a power of 2
// CHECK: Preferred WaveSize value 32 must be between 15 and 17
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(15, 17, 32)]
void node04(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: WaveSize arguments must be between 4 and 128 and a power of 2
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(-15, 16, 8)]
void node05(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: 'WaveSize' attribute takes no more than 3 arguments
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(4, 16, 8, 8)]
void node06(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: 'WaveSize' attribute takes at least 1 argument
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize()]
void node07(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: 'WaveSize' attribute requires an integer constant
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(4, 8, node07)]
void node08(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: shader attribute type 'wavesize' conflicts with shader attribute type 'wavesize'
// CHECK: note: conflicting attribute is here
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(8, 32, 8)]
[WaveSize(4, 32, 8)]
void node09(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: shader attribute type 'wavesize' conflicts with shader attribute type 'wavesize'
// CHECK: note: conflicting attribute is here
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(8, 32, 8)]
[WaveSize(8, 16, 8)]
void node10(DispatchNodeInputRecord<INPUT_RECORD> input) { }

// CHECK: error: shader attribute type 'wavesize' conflicts with shader attribute type 'wavesize'
// CHECK: note: conflicting attribute is here
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(4, 8, 8)]
[WaveSize(4, 8, 4)]
void node11(DispatchNodeInputRecord<INPUT_RECORD> input) { }


// CHECK: error: shader attribute type 'wavesize' conflicts with shader attribute type 'wavesize'
// CHECK: note: conflicting attribute is here
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(32,1,1)]
[WaveSize(4, 8, 4)]
[WaveSize(4)]
void node12(DispatchNodeInputRecord<INPUT_RECORD> input) { }
