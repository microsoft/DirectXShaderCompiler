// Source file for unaltered known_nodeio_tags.ll
// and the altered unknown_nodeio_tags.ll
// Not intended for indpendent testing

struct MY_INPUT_RECORD {
  float foo;
};

struct MY_RECORD {
  float bar;
};

struct MY_MATERIAL_RECORD {
  uint textureIndex;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2,3,2)]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void main([MaxRecords(7)] DispatchNodeInputRecord<MY_INPUT_RECORD> myInput,
              [NodeArraySize(42)]
              [MaxRecords(7)]
              NodeOutput<MY_RECORD> myFascinatingNode,
              [MaxRecordsSharedWith(myFascinatingNode)]
              [AllowSparseNodes]
              [NodeArraySize(63)] NodeOutputArray<MY_MATERIAL_RECORD> myMaterials)
{
  // Don't really need to do anything
}
