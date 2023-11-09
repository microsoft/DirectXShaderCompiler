// RUN: %dxc -T cs_6_6 -verify %s 
// check that an error gets generated each time a "node" attribute gets used without the presence of 
// the '[shader("node")]' attribute

[Shader("compute")]
[NodeIsProgramEntry] // expected-error{{attribute NodeIsProgramEntry only allowed on node shaders}}
[NodeID("nodeName")] // expected-error{{attribute NodeId only allowed on node shaders}}
[NodeLocalRootArgumentsTableIndex(3)] // expected-error{{attribute NodeLocalRootArgumentsTableIndex only allowed on node shaders}}
[NodeShareInputOf("nodeName")] // expected-error{{attribute NodeShareInputOf only allowed on node shaders}}
[NodeMaxRecursionDepth(31)] // expected-error{{attribute NodeMaxRecursionDepth only allowed on node shaders}}
[NodeDispatchGrid(1, 1, 1)] // expected-error{{attribute NodeDispatchGrid only allowed on node shaders}}
[NodeMaxDispatchGrid(2,2,2)] // expected-error{{attribute NodeMaxDispatchGrid only allowed on node shaders}}
[NumThreads(1, 1, 1)]
void secondNode()
{    
}
