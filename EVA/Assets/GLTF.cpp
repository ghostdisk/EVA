#include <EVA/Core/Serialization.hpp>
#include <EVA/Entities/Entity.hpp>
#include <EVA/Assets/Asset.hpp>
#include <EVA/GFX/Mesh.hpp>
#include <EVA/Assets/Model.hpp>
#include <EVA/Assets/Material.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Model.hpp>
#include <EVA/GFX/Mesh.hpp>
#include <EVA/Assets/GLTF.hpp>

#define CGLTF_IMPLEMENTATION
#include <Vendor/cgltf.h>

Result GLTF::LoadImpl(Deserializer& d) {
	ScratchArena scratch;

	cgltf_options options = {};
	cgltf_data* data = nullptr;

	if (cgltf_parse_file(&options, m_fsPath.c_str(), &data) != cgltf_result_success)
		return Err("cgltf_parse_file: failed to load %s", m_fsPath.c_str());
	// DEFER(cgltf_free(data));

	if (cgltf_load_buffers(&options, data, m_fsPath.c_str()) != cgltf_result_success) {
		return Err("cgltf_load_buffers: failed to load %s", m_fsPath.c_str());
	}

	// String nameWithoutExt = FS::WithoutExtension(m_name);
	String fsPathWithoutExt = FS::WithoutExtension(m_fsPath);

	// String mdlName = scratch->Fmt("%s.mdl", STRING_PRINTF_ARGS(nameWithoutExt));
	ZTString mdlFsPath = scratch->Fmt("%.*s.mdl", STRING_PRINTF_ARGS(fsPathWithoutExt));

	ModelData modelData;

	// Model* model = Asset::Get<Model>(mdlName);
	// model->meshes.clear();

	/*
	for (int material_idx = 0; material_idx < data->materials_count; material_idx++) {
		Material* material = new Material();
		// gltf->materials.push_back(material);

		cgltf_material& gltf_material = data->materials[material_idx];

		cgltf_texture* in_texture = gltf_material.pbr_metallic_roughness.base_color_texture.texture;
		if (in_texture) {
			if (in_texture->image) {
				material->color_texture = (Texture*)AssetGetByName(in_texture->image->name, Texture::StaticClass());
				if (!material->color_texture)
				{
					fprintf(stderr, "[warn] missing texture %s\n", in_texture->image->name);
				}
			}
		}
	}
	*/

	for (int mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
		cgltf_mesh& gltf_mesh = data->meshes[mesh_idx];
		if (gltf_mesh.primitives_count > 1)
			fprintf(stderr, "mesh %d has %d primitives, all but the first are ignored.\n", mesh_idx, (int)gltf_mesh.primitives_count);

		cgltf_primitive& primitive = gltf_mesh.primitives[0];

		cgltf_attribute* attr_pos = nullptr;
		cgltf_attribute* attr_nrm = nullptr;
		cgltf_attribute* attr_uv0 = nullptr;

		for (int i = 0; i < primitive.attributes_count; i++) {
			cgltf_attribute* attr = &primitive.attributes[i];
			switch (attr->type) {
				case cgltf_attribute_type_position: attr_pos = attr; break;
				case cgltf_attribute_type_normal:   attr_nrm = attr; break;
				case cgltf_attribute_type_texcoord: attr_uv0 = attr; break;
				default: break;
			}
		}
		if (!attr_pos || !primitive.indices) return Err("Invalid gltf");

		MeshData meshData = {};

		meshData.vertices.resize(attr_pos->data->count);

		Vector<float3> pos(attr_pos->data->count);
		Vector<float3> nrm(attr_pos->data->count);
		Vector<float2> uv0(attr_pos->data->count);

		if (1       ) cgltf_accessor_unpack_floats(attr_pos->data, (float*)pos.data(), pos.size() * 3);
		if (attr_nrm) cgltf_accessor_unpack_floats(attr_nrm->data, (float*)nrm.data(), nrm.size() * 3);
		if (attr_uv0) cgltf_accessor_unpack_floats(attr_uv0->data, (float*)uv0.data(), uv0.size() * 2);

		for (int i = 0; i < meshData.vertices.size(); i++) {
			meshData.vertices[i].position = pos[i];
			meshData.vertices[i].normal   = nrm[i];
			meshData.vertices[i].texcoord = uv0[i];
		}

		meshData.indices.resize(primitive.indices->count);
		cgltf_accessor_unpack_indices(primitive.indices, meshData.indices.data(), 4, meshData.indices.size());

		/*
		char mesh_name[64];
		if (gltf_mesh.name) {
			snprintf(mesh_name, 64, "%s/meshes/%s", name, gltf_mesh.name);
		} else {
			snprintf(mesh_name, 64, "%s/meshes/mesh%d", name, mesh_idx);
		}
		*/

		modelData.meshes.push_back({
			.name = "mesh123",
			.meshData = Move(meshData),
		});
		/*
		if (primitive.material) {
			mesh->default_material = gltf->materials[cgltf_material_index(data, primitive.material)];
		}
		gltf->meshes.push_back(mesh);
		*/
	}

	/*
	for (int scene_idx = 0; scene_idx < data->scenes_count; scene_idx++) {
		cgltf_scene& gltf_scene = data->scenes[scene_idx];
		GLTFScene* scene = new GLTFScene();
		gltf->scenes.push_back(scene);

		for (int node_idx = 0; node_idx < gltf_scene.nodes_count; node_idx++) {
			cgltf_node* gltf_node = gltf_scene.nodes[node_idx];
			if (gltf_node->parent) {
				fprintf(stderr, "[warn] gltf node has parent, this is not implementerd, result may be wrong\n");
			}

			Mesh* mesh = nullptr;
			Material* material = nullptr;

			if (gltf_node->mesh) {
				mesh = gltf->meshes[cgltf_mesh_index(data, gltf_node->mesh)];
				material = mesh->default_material;
			}

			scene->nodes.push_back(GLTFSceneNode{
				.position = float3(gltf_node->translation),
				.rotation = float4(gltf_node->rotation),
				.scale    = float3(gltf_node->scale),
				.mesh     = mesh,
				.material = material,
			});

			if (gltf_node->name) {
				snprintf(scene->nodes.back().name, 16, "%s", gltf_node->name);
			}
		}
	}
	*/

	FILE* outFile = fopen(mdlFsPath.c_str(), "wb");
	if (!outFile) {
		return Err("GLTF: Failed to open %s for writing", mdlFsPath.c_str());
	}
	TextSerializer s(outFile);
	Serialize(s, modelData);
	fclose(outFile);
	return Success();
}