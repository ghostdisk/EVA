#include <EVA/GLTF.hpp>

#define CGLTF_IMPLEMENTATION
#include <Vendor/cgltf.h>

GLTF* GLTFLoad(const char* name)
{
	GLTF* gltf = new GLTF();

	char path[256];
	snprintf(path, sizeof(path), "%s/Assets/%s.glb", EVA_BASE_DIR, name);

	cgltf_options options = {};
	cgltf_data* data = nullptr;
	if (cgltf_parse_file(&options, path, &data) != cgltf_result_success)
	{
		Fatal("cgltf_parse_file: failed to load %s", path);
	}

	DEFER(cgltf_free(data));
	if (cgltf_load_buffers(&options, data, path) != cgltf_result_success)
	{
		Fatal("cgltf_load_buffers: failed to load %s", path);
	}


	for (int mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++)
	{
		cgltf_mesh& gltf_mesh = data->meshes[mesh_idx];
		if (gltf_mesh.primitives_count > 1)
		{
			fprintf(stderr, "[warn] GLTFLoad: %s mesh %d has %d primitives, all but the first are ignored.\n",
				name, mesh_idx, (int)gltf_mesh.primitives_count);
		}

		cgltf_primitive& primitive = gltf_mesh.primitives[0];

		cgltf_attribute* attr_pos = nullptr;
		cgltf_attribute* attr_nrm = nullptr;
		cgltf_attribute* attr_uv0 = nullptr;

		for (int i = 0; i < primitive.attributes_count; i++)
		{
			cgltf_attribute* attr = &primitive.attributes[i];
			switch (attr->type)
			{
				case cgltf_attribute_type_position: attr_pos = attr; break;
				case cgltf_attribute_type_normal:   attr_nrm = attr; break;
				case cgltf_attribute_type_texcoord: attr_uv0 = attr; break;
				default: break;
			}
		}
		if (!attr_pos) Fatal("Invalid gltf %s\n", name);

		std::vector<MeshVertex> vertices(attr_pos->data->count);

		std::vector<float3> pos(attr_pos->data->count);
		std::vector<float3> nrm(attr_nrm->data->count);
		std::vector<float2> uv0(attr_uv0->data->count);

		if (1       ) cgltf_accessor_unpack_floats(attr_pos->data, (float*)pos.data(), pos.size() * 3);
		if (attr_nrm) cgltf_accessor_unpack_floats(attr_nrm->data, (float*)nrm.data(), nrm.size() * 3);
		if (attr_uv0) cgltf_accessor_unpack_floats(attr_uv0->data, (float*)uv0.data(), uv0.size() * 2);

		for (int i = 0; i < vertices.size(); i++)
		{
			vertices[i].position = pos[i];
			vertices[i].normal   = nrm[i];
			vertices[i].texcoord = uv0[i];
		}

		std::vector<U32> indices(primitive.indices->count);
		cgltf_accessor_unpack_indices(primitive.indices, indices.data(), 4, indices.size());

		char mesh_name[64];
		snprintf(mesh_name, 64, "%s/meshes/%d", name, mesh_idx);

		gltf->meshes.push_back(CreateMesh(mesh_name,
			vertices.size(), vertices.data(),
			indices.size(), indices.data()));
	}

	return gltf;
}