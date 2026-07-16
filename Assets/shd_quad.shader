{
	version = 3
	vs = "DrawBox.vs.glsl"
	fs = "DrawBox.fs.glsl"
	defines = [:0
	]
	pipelineState = {
		version = 4
		cullMode = "Front"
		blendMode = "AlphaBlend"
		topology = "TriangleList"
		depthTest = false
	}
}
