#include <EVA/Assets/Model.hpp>
#include <EVA/GFX/Mesh.hpp>
#include <EVA/Core/Serialization.hpp>
#include <stdio.h>


Result Model::LoadImpl(Deserializer& d) {
	fprintf(stderr, "Model::LoadImpl STUB\n");
	return Success();
}

