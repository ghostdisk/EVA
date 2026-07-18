{
	version = 3
	vs = "/Shaders/Main.vs.glsl"
	fs = "/Shaders/Main.fs.glsl"
	defines = [:1
		"S_BRUSH"
	]
	pipelineState = {
		version = 4
		cullMode = "Back"
		blendMode = "Solid"
		topology = "TriangleList"
		depthTest = true
	}
}