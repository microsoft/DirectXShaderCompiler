// Test that an error is emitted when a SubpassInput variable is used in
// something other than a pixel shader.

// RUN: %dxc -T cs_6_0 -E CsTest

[[vk::input_attachment_index (0)]] SubpassInput<float4> subInput;
[numthreads (8,1,1)]
void CsTest() {
	subInput.SubpassLoad();
}

// CHECK: :9:2: error: SubpassInput(MS) only allowed in pixel shader
