{
	version = 3
	vs = "/Shaders/Lines.vs.glsl"
	fs = "/Shaders/Lines.fs.glsl"
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