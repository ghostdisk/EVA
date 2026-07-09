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

void Deserialize(Deserializer& d, Model* model) {
	d.BeginObject();

	d.Key("version");
	U32 version = d.DeserializeU32();
	if (version != 1) {
		d.res = Err("unexpected version");
		return;
	}

	d.Key("meshes");
	int num_meshes = d.BeginArray();
	model->meshes.resize(num_meshes);

	for (int i = 0; i < num_meshes; i++) {
		model->meshes[i] = new Mesh();
		Deserialize(d, model->meshes[i]);
		if (d.res.error)
			return;
		model->meshes[i]->Upload();
	}
	d.EndArray();

	d.EndObject();
}

Result Model::LoadImpl(FILE* f) {
	TextDeserializer d(f);
	Deserialize(d, this);
	return d.res;
}

Result Model::SaveToDisk(ZTString path) {
	FILE* f = fopen(path, "wb");
	if (!f) return Err("Failed to open %s", path.c_str());
	DEFER(fclose(f));

	TextSerializer s(f);
	Serialize(s, this);

	return Success();
}