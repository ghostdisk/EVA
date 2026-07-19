{
	version = 3
	vs = "/ShaderFiles/DrawBox.vs.glsl"
	fs = "/ShaderFiles/DrawBox.fs.glsl"
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