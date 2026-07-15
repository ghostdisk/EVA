{
	version = 3
	vs = "Main.vs.glsl"
	fs = "Main.fs.glsl"
	defines = [:1
		"S_BRUSH"
	]
	pipelineState = {
		version = 2
		cullMode = Back
		blendMode = Solid
	}
}
