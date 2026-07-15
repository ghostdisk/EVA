{
	version = 3
	vs = "DrawBox.vs.glsl"
	fs = "DrawBox.fs.glsl"
	defines = [:0
	]
	pipelineState = {
		version = 3
		cullMode = Front
		blendMode = AlphaBlend
		depthTest = false
	}
}
