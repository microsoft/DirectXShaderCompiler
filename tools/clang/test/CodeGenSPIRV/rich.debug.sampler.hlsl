// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

//CHECK: [[name:%\d+]] = OpString "@type.sampler"
//CHECK: [[info_none:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugInfoNone
//CHECK: [[sampler:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name]] Structure [[src:%\d+]] 0 0 [[cu:%\d+]] {{%\d+}} [[info_none]]

//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[sampler]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %s1
SamplerState           s1 : register(s1);
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[sampler]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %s2
SamplerComparisonState s2 : register(s2);
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[sampler]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %s3
sampler                s3 : register(s3);

void main() {
}
