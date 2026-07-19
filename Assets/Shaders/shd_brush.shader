{
	version = 3
	vs = "/ShaderFiles/Main.vs.glsl"
	fs = "/ShaderFiles/Main.fs.glsl"
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