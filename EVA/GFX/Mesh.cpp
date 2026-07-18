#include <EVA/Core/Serialization.hpp>
#include <EVA/GFX/Mesh.hpp>

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

void Mesh::Upload(const MeshData& data) {
	m_indexCount = data.indices.size();
	m_vertexCount = data.vertices.size();

	GPUDevice* device = GPUDevice::Get();
	U64 vertexDataSize = sizeof(MeshVertex) * m_vertexCount;
	m_vertexBuffer = device->CreateBuffer({
		.name = "vertex buffer",
		.size = vertexDataSize,
		.usage = GPUBufferUsage_TransferDest | GPUBufferUsage_StorageBuffer,
		.memoryUsage = GPUMemoryUsage::GPUOnly,
		.bindless = true,
	});
	device->UploadBuffer(m_vertexBuffer, vertexDataSize, 0, data.vertices.data());

	if (m_indexCount) {
		U64 indexDataSize = sizeof(U32) * m_indexCount;
		m_indexBuffer = device->CreateBuffer({
			.name = "index buffer",
			.size = indexDataSize,
			.usage = GPUBufferUsage_TransferDest | GPUBufferUsage_IndexBuffer,
			.memoryUsage = GPUMemoryUsage::GPUOnly,
		});
		device->UploadBuffer(m_indexBuffer, indexDataSize, 0, data.indices.data());
	}
}

void Mesh::Unload() {
	GPUDevice* device = GPUDevice::Get();
	if (m_vertexBuffer) {
		device->DestroyBuffer(m_vertexBuffer);
		m_vertexBuffer = nullptr;
	}
	if (m_indexBuffer) {
		device->DestroyBuffer(m_indexBuffer);
		m_indexBuffer = nullptr;
	}
}

void Serialize(Serializer& s, const MeshData& data) {
	s.BeginObject();

	s.Key("version");
	s.SerializeU32(1);

	s.Key("vertices");
	s.BeginArray(data.vertices.size());
	for (const MeshVertex& vertex : data.vertices) {
		Serialize(s, vertex);
	}
	s.EndArray();

	s.Key("indices");
	s.BeginArray(data.indices.size());
	for (const U32& index : data.indices) {
		Serialize(s, index);
	}
	s.EndArray();

	s.EndObject();
}

void Deserialize(Deserializer& d, MeshData& data) {
	d.BeginObject();

	d.Key("version");
	U32 version = d.DeserializeU32();
	if (version != 1) {
		d.SetError(Err("unexpected version"));
		return;
	}

	d.Key("vertices");
	U32 num_vertices = d.BeginArray();
	data.vertices.resize(num_vertices);
	for (int i = 0; i < num_vertices; i++) {
		Deserialize(d, data.vertices[i]);
	}
	d.EndArray();

	d.Key("indices");
	U32 num_indices = d.BeginArray();
	data.indices.resize(num_indices);
	for (int i = 0; i < num_indices; i++) {
		Deserialize(d, data.indices[i]);
	}
	d.EndArray();

	d.EndObject();
}