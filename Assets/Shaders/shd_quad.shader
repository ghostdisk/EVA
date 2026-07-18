{
	version = 3
	vs = "/Shaders/DrawBox.vs.glsl"
	fs = "/Shaders/DrawBox.fs.glsl"
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