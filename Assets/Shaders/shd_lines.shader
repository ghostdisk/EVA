{
	version = 3
	vs = "/ShaderFiles/Lines.vs.glsl"
	fs = "/ShaderFiles/Lines.fs.glsl"
	defines = [:0
	]
	pipelineState = {
		version = 4
		cullMode = "Back"
		blendMode = "Solid"
		topology = "LineList"
		depthTest = true
	}
}