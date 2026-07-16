{
	version = 3
	vs = "Lines.vs.glsl"
	fs = "Lines.fs.glsl"
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
