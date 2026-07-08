#include <EVA/Core/Serialization.hpp>
#include <EVA/Assets/Mesh.hpp>

void Serialize(Serializer& s, const MeshVertex& f) {
	s.BeginObject();
	s.Key("position");
	Serialize(s, f.position);
	s.Key("normal");
	Serialize(s, f.normal);
	s.Key("texcoord");
	Serialize(s, f.texcoord);
	s.EndObject();
}

void Deserialize(Deserializer& d, MeshVertex& f) {
	d.BeginObject();
	d.Key("position");
	Deserialize(d, f.position);
	d.Key("normal");
	Deserialize(d, f.normal);
	d.Key("texcoord");
	Deserialize(d, f.texcoord);
	d.EndObject();
}

void Mesh::InitCPUData(size_t num_vertices, const MeshVertex* vertices, size_t num_indices, const U32* indices) {
	this->vertices.resize(num_vertices);
	this->indices.resize(num_vertices);
	memcpy(this->vertices.data(), vertices, this->vertices.size() * sizeof(vertices[0]));
	memcpy(this->indices.data(), vertices, this->indices.size() * sizeof(indices[0]));
}

void Serialize(Serializer& s, Mesh* value) {
	s.BeginObject();

	s.Key("version");
	s.SerializeU32(1);

	s.Key("vertices");
	s.BeginArray(value->vertices.size());
	for (const MeshVertex& vertex : value->vertices) {
		Serialize(s, vertex);
	}
	s.EndArray();


	s.EndObject();
}

void Deserialize(Serializer& s, Mesh* value) {
}