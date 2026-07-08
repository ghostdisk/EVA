#include <EVA/Assets/Model.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/Core/Serialization.hpp>
#include <stdio.h>

void Serialize(Serializer& s, Model* model) {
	s.BeginObject();

	s.Key("version");
	s.SerializeU32(1);

	s.Key("meshes");
	s.BeginArray(model->meshes.size());

	for (Mesh* mesh : model->meshes) {
		Serialize(s, mesh);
	}

	s.EndArray();
	s.EndObject();
}

Result Model::LoadImpl(const U8* file, size_t file_size) {
	return Success();
}

Result Model::SaveToDisk(ZTString path) {
	FILE* f = fopen(path, "wb");
	if (!f) return Err("Failed to open %s", path.c_str());
	DEFER(fclose(f));

	TextSerializer s(f);
	Serialize(s, this);
}