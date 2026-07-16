#include <EVA/Assets/Asset.hpp>
#include <EVA/Assets/Sprite.hpp>
#include <EVA/Assets/Texture.hpp>
#include <EVA/Assets/Font.hpp>
#include <EVA/Assets/Shader.hpp>
#include <EVA/Assets/Material.hpp>
#include <EVA/Assets/Mesh.hpp>
#include <EVA/Game.hpp>
#include <EVA/GFX/Renderer.hpp>
#include <EVA/Platform.hpp>
#include <EVA/Library.hpp>
#include <EVA/Console.hpp>
#include <tracy/Tracy.hpp>

// ------------------------------------------------------------

struct LineVertex {
	float3 position;
	float4 color;
};

struct DrawMeshEntry {
	Mesh*     mesh;
	Material* material;
	float4x4  matrix;
	float4    color;
};

enum DrawCommandType {
	DrawCommandType_None,
	DrawCommandType_Lines,
	DrawCommandType_Quads,
	DrawCommandType_Meshes,
};

struct LayerData {
	Vector<LineVertex> lines;
	Vector<DrawMeshEntry> meshes;
	Vector<DrawQuadRecord> quads;
};

struct MainConstantBuffer {
	float4x4 view;
	float4x4 view_projection;
};

// ------------------------------------------------------------

static GraphicsDevice* g_device;

Shader* shd_lines;
Shader* shd_main;
Shader* shd_quad;
Shader* shd_brush;

static Mesh* mesh_quad = nullptr;
static GPUBuffer* g_mainConstantBuffer = nullptr;
static Image* g_depthBuffer = nullptr;

static LayerData layers[Layer_ENUM_SIZE] = {};
static LayerData* current_layer = &layers[Layer_Main];

// ------------------------------------------------------------

ConVar cvar_wireframe = {
	.name = "r_wireframe",
	.help = "toggle wireframe rendering",
	.fvalue = 0,
};

static void CreateBuffers() {
	Image* backbuffer = g_device->GetCurrentBackbuffer();

	g_depthBuffer = g_device->CreateImage({
		.name = "depth buffer",
		.width = backbuffer->m_width,
		.height = backbuffer->m_height,
		.format = Format::D32_FLOAT,
		.usage = ImageUsage_DepthAttachment,
	});
}

static void DestroyBuffers() {
	if (g_depthBuffer) { g_device->DestroyImage(g_depthBuffer); g_depthBuffer = nullptr; }
}

void RendererInitialize1() {
	g_device = GraphicsDevice::Get();

	{ // mesh_quad:
		MeshVertex quad_vertices[] = {
			MeshVertex { .position = float3(0, 0, 0) },
			MeshVertex { .position = float3(1, 0, 0) },
			MeshVertex { .position = float3(1, 1, 0) },
			MeshVertex { .position = float3(0, 1, 0) },
		};
		U32 quad_indices[] = { 0, 1, 2, 0, 2, 3 };
		mesh_quad = new Mesh();
		mesh_quad->InitCPUData(
			EVA_ARRAYSIZE(quad_vertices), quad_vertices,
			EVA_ARRAYSIZE(quad_indices), quad_indices);
		mesh_quad->Upload(false);
	}

	{ // create standard samplers:
		g_device->CreateSampler({
			.minFilter  = Filter::Nearest,
			.magFilter  = Filter::Nearest,
			.mipmapMode = MipmapMode::Nearest,
			.addressU    = AddressMode::ClampToEdge,
			.addressV    = AddressMode::ClampToEdge,
			.addressW    = AddressMode::ClampToEdge,
			.bindless    = true,
			.forcedBindlessIndex = (U32)StandardSampler::PointClamp,
		});
		g_device->CreateSampler({
			.minFilter  = Filter::Nearest,
			.magFilter  = Filter::Nearest,
			.mipmapMode = MipmapMode::Nearest,
			.addressU    = AddressMode::Repeat,
			.addressV    = AddressMode::Repeat,
			.addressW    = AddressMode::Repeat,
			.bindless    = true,
			.forcedBindlessIndex = (U32)StandardSampler::PointWrap,
		});
		g_device->CreateSampler({
			.minFilter  = Filter::Nearest,
			.magFilter  = Filter::Nearest,
			.mipmapMode = MipmapMode::Nearest,
			.addressU    = AddressMode::MirroredRepeat,
			.addressV    = AddressMode::MirroredRepeat,
			.addressW    = AddressMode::MirroredRepeat,
			.bindless    = true,
			.forcedBindlessIndex = (U32)StandardSampler::PointMirror,
		});
		g_device->CreateSampler({
			.mipmapMode = MipmapMode::Nearest,
			.addressU    = AddressMode::ClampToEdge,
			.addressV    = AddressMode::ClampToEdge,
			.addressW    = AddressMode::ClampToEdge,
			.bindless    = true,
			.forcedBindlessIndex = (U32)StandardSampler::LinearClamp,
		});
		g_device->CreateSampler({
			.mipmapMode = MipmapMode::Nearest,
			.bindless    = true,
			.forcedBindlessIndex = (U32)StandardSampler::LinearWrap,
		});
		g_device->CreateSampler({
			.mipmapMode = MipmapMode::Nearest,
			.addressU    = AddressMode::MirroredRepeat,
			.addressV    = AddressMode::MirroredRepeat,
			.addressW    = AddressMode::MirroredRepeat,
			.bindless    = true,
			.forcedBindlessIndex = (U32)StandardSampler::LinearMirror,
		});
		g_device->CreateSampler({
			.addressU = AddressMode::ClampToEdge,
			.addressV = AddressMode::ClampToEdge,
			.addressW = AddressMode::ClampToEdge,
			.bindless = true,
			.forcedBindlessIndex = (U32)StandardSampler::TrilinearClamp,
		});
		g_device->CreateSampler({
			.bindless = true,
			.forcedBindlessIndex = (U32)StandardSampler::TrilinearWrap,
		});
		g_device->CreateSampler({
			.addressU = AddressMode::MirroredRepeat,
			.addressV = AddressMode::MirroredRepeat,
			.addressW = AddressMode::MirroredRepeat,
			.bindless = true,
			.forcedBindlessIndex = (U32)StandardSampler::TrilinearMirror,
		});
		g_device->CreateSampler({
			.addressU         = AddressMode::ClampToEdge,
			.addressV         = AddressMode::ClampToEdge,
			.addressW         = AddressMode::ClampToEdge,
			.maxAnisotropy    = 16.0f,
			.anisotropyEnable = true,
			.bindless         = true,
			.forcedBindlessIndex = (U32)StandardSampler::AnisoClamp,
		});
		g_device->CreateSampler({
			.maxAnisotropy    = 16.0f,
			.anisotropyEnable = true,
			.bindless         = true,
			.forcedBindlessIndex = (U32)StandardSampler::AnisoWrap,
		});
		g_device->CreateSampler({
			.addressU         = AddressMode::MirroredRepeat,
			.addressV         = AddressMode::MirroredRepeat,
			.addressW         = AddressMode::MirroredRepeat,
			.maxAnisotropy    = 16.0f,
			.anisotropyEnable = true,
			.bindless         = true,
			.forcedBindlessIndex = (U32)StandardSampler::AnisoMirror,
		});
		g_device->CreateSampler({
			.minFilter     = Filter::Nearest,
			.magFilter     = Filter::Nearest,
			.mipmapMode    = MipmapMode::Nearest,
			.addressU       = AddressMode::ClampToEdge,
			.addressV       = AddressMode::ClampToEdge,
			.addressW       = AddressMode::ClampToEdge,
			.compareEnable  = true,
			.compareOp      = CompareOp::LessEqual,
			.bindless       = true,
			.forcedBindlessIndex = (U32)StandardSampler::ShadowPointClamp,
		});
		g_device->CreateSampler({
			.addressU      = AddressMode::ClampToEdge,
			.addressV      = AddressMode::ClampToEdge,
			.addressW      = AddressMode::ClampToEdge,
			.compareEnable = true,
			.compareOp     = CompareOp::LessEqual,
			.bindless      = true,
			.forcedBindlessIndex = (U32)StandardSampler::ShadowLinearClamp,
		});
	}

	ConRegisterVar(&cvar_wireframe);

	g_mainConstantBuffer = g_device->CreateGPUBuffer({
		.name        = "main constant buffer",
		.size        = sizeof(MainConstantBuffer),
		.usage       = GPUBufferUsage_StorageBuffer | GPUBufferUsage_TransferDest,
		.memoryUsage = MemoryUsage::GPUOnly,
		.bindless    = true,
	});

	CreateBuffers();
}

void RendererInitialize2() {
	shd_lines = Asset::Get<Shader>("shd_lines");
	shd_main  = Asset::Get<Shader>("shd_main");
	shd_quad  = Asset::Get<Shader>("shd_quad");
	shd_brush = Asset::Get<Shader>("shd_brush");
}

void RendererShutdown() {
	g_device->DestroyImage(g_depthBuffer);
	g_depthBuffer = nullptr;
}

void DrawLine(float3 a, float3 b, float4 color) {
	current_layer->lines.push_back({ a, color });
	current_layer->lines.push_back({ b, color });
}

void DrawSetLayer(Layer l) {
	current_layer = &layers[l];
}

void RenderFrame() {
	ZoneScopedN("RenderFrame");

	Image* backbuffer = g_device->GetCurrentBackbuffer();

	bool buffersDirty = false;
	if (!g_depthBuffer) buffersDirty = true;
	else if (g_depthBuffer->m_width != backbuffer->m_width || g_depthBuffer->m_height != backbuffer->m_height) buffersDirty = true;

	if (buffersDirty) {
		DestroyBuffers();
		CreateBuffers();
	}

	CommandBuffer* cmd = g_device->GetMainCommandBuffer();

	ECamera* camera = nullptr;
	if (g_active_game && g_active_game->m_activeCamera) {
		camera = g_active_game->m_activeCamera;
	}

	MainConstantBuffer cb = {};
	if (camera) {
		cb.view = camera->view_matrix;
		cb.view_projection = camera->view_projection_matrix;
	}
	g_device->UploadBuffer(g_mainConstantBuffer, sizeof(cb), 0, &cb);

	AttachmentDesc colorAttachment = {
		.image       = backbuffer,
		.loadOp      = LoadOp::Clear,
		.storeOp     = StoreOp::Store,
		.clearColor  = COLOR_BLACK,
		.stateBefore = ImageState::Undefined,
		.stateDuring = ImageState::ColorAttachment,
		.stateAfter  = ImageState::Present,
	};
	AttachmentDesc depthAttachment = {
		.image       = g_depthBuffer,
		.loadOp      = LoadOp::Clear,
		.storeOp     = StoreOp::Store,
		.clearDepth  = 1.0f,
		.stateBefore = g_depthBuffer->m_state,
		.stateDuring = ImageState::DepthAttachment,
		.stateAfter  = ImageState::DepthAttachment,
	};
	RenderingDesc renderingDesc = {
		.colorAttachmentCount = 1,
		.colorAttachments = &colorAttachment,
		.depthAttachment = &depthAttachment,
	};
	cmd->BeginRendering(renderingDesc);

	for (Layer layer = (Layer)0; layer < Layer_ENUM_SIZE; layer = (Layer)(layer + 1)) {
		DrawSetLayer(layer);

		{ // render pending meshes:
			for (const DrawMeshEntry& entry : current_layer->meshes)
			{
				Material* material      = entry.material;
				Shader*   shader        = shd_main;
				Texture*  color_texture = Library::tex_proto;

				if (!material) {
					material = entry.mesh->default_material;
				}
				if (material) {
					shader = material->shader;
					if (material->color_texture) 
						color_texture = material->color_texture;
				}

				struct MeshPushConstants {
					float4x4 model;
					float4 color;
					float textureScale;
					U32 cameraBuffer;
					U32 vertexBuffer;
					U32 colorImage;
					U32 colorSampler;
				} constants = {
					.model = entry.matrix,
					.color = entry.color,
					.textureScale = material ? material->texture_scale : 1.0f,
					.cameraBuffer = g_mainConstantBuffer->m_bindlessIndex,
					.vertexBuffer = entry.mesh->vertex_buffer->m_bindlessIndex,
					.colorImage = color_texture->image->m_bindlessIndex,
					.colorSampler = color_texture->m_sampler->m_bindlessIndex,
				};
				cmd->BindPipeline(shader->m_pipeline);
				cmd->PushConstants(shader->m_pipeline, sizeof(constants), &constants);

				if (entry.mesh->index_count) {
					cmd->BindIndexBuffer(entry.mesh->index_buffer, IndexType::U32);
					cmd->DrawIndexed(entry.mesh->index_count);
				} else {
					cmd->Draw(entry.mesh->vertex_count);
				}
			}

			current_layer->meshes.clear();
		}

		// render pending lines:
		if (current_layer->lines.size()) { 
			GPUBuffer* lineBuffer = nullptr;
			size_t lineBufferOffset = 0;
			g_device->UploadStagingData(current_layer->lines.data(), current_layer->lines.size() * sizeof(LineVertex), &lineBuffer, &lineBufferOffset);
			struct LinePushConstants {
				U32 cameraBuffer;
				U32 vertexBuffer;
				U32 vertexOffset;
			} constants = {
				g_mainConstantBuffer->m_bindlessIndex,
				lineBuffer->m_bindlessIndex,
				(U32)(lineBufferOffset / sizeof(LineVertex)),
			};
			cmd->BindPipeline(shd_lines->m_pipeline);
			cmd->PushConstants(shd_lines->m_pipeline, sizeof(constants), &constants);
			cmd->Draw((U32)current_layer->lines.size());

			current_layer->lines.clear();
		}

		// render quads:
		{
			Vector<DrawQuad> quads(current_layer->quads.size());
			for (int i = 0; i < current_layer->quads.size(); i++) {
				DrawQuadRecord& record = current_layer->quads[i];
				quads[i] = DrawQuad{
					.mode          = record.mode,
					.texture       = record.texture ? record.texture->image->m_bindlessIndex : 0,
					.sampler       = record.texture ? record.texture->m_sampler->m_bindlessIndex : 8,
					.position_rect = record.position_rect,
					.uv_rect       = record.uv_rect,
					.tint          = record.tint,
				};
			}

			GPUBuffer* quadBuffer = nullptr;
			size_t quadBufferOffset = 0;
			g_device->UploadStagingData(quads.data(), quads.size() * sizeof(DrawQuad), &quadBuffer, &quadBufferOffset);
			struct QuadPushConstants {
				float2 framebufferSize;
				U32 quadBuffer;
				U32 quadOffset;
				U32 vertexBuffer;
			} constants = {
				.framebufferSize = float2(colorAttachment.image->m_width, colorAttachment.image->m_height),
				.quadBuffer = quadBuffer->m_bindlessIndex,
				.quadOffset = (U32)(quadBufferOffset / sizeof(DrawQuad)),
				.vertexBuffer = mesh_quad->vertex_buffer->m_bindlessIndex,
			};
			cmd->BindPipeline(shd_quad->m_pipeline);
			cmd->PushConstants(shd_quad->m_pipeline, sizeof(constants), &constants);
			cmd->BindIndexBuffer(mesh_quad->index_buffer, IndexType::U32);
			cmd->DrawIndexed(6, (U32)quads.size());

			current_layer->quads.clear();
		}

	}
	cmd->EndRendering(renderingDesc);
}

void DrawMesh(Mesh* mesh, Material* material, const float4x4& matrix, float4 color) {
	current_layer->meshes.push_back({ mesh, material, matrix, color });
}

void DrawAABB(float3 center, float3 size, float4 color) {
	float3 hsize = size / 2.0f;
	DrawLine(center + float3(-hsize.x, -hsize.y, -hsize.z), center + float3( hsize.x, -hsize.y, -hsize.z), color);
	DrawLine(center + float3(-hsize.x,  hsize.y, -hsize.z), center + float3( hsize.x,  hsize.y, -hsize.z), color);
	DrawLine(center + float3(-hsize.x, -hsize.y, -hsize.z), center + float3(-hsize.x,  hsize.y, -hsize.z), color);
	DrawLine(center + float3( hsize.x, -hsize.y, -hsize.z), center + float3( hsize.x,  hsize.y, -hsize.z), color);

	DrawLine(center + float3(-hsize.x, -hsize.y,  hsize.z), center + float3( hsize.x, -hsize.y,  hsize.z), color);
	DrawLine(center + float3(-hsize.x,  hsize.y,  hsize.z), center + float3( hsize.x,  hsize.y,  hsize.z), color);
	DrawLine(center + float3(-hsize.x, -hsize.y,  hsize.z), center + float3(-hsize.x,  hsize.y,  hsize.z), color);
	DrawLine(center + float3( hsize.x, -hsize.y,  hsize.z), center + float3( hsize.x,  hsize.y,  hsize.z), color);

	DrawLine(center + float3(-hsize.x, -hsize.y, -hsize.z), center + float3(-hsize.x, -hsize.y,  hsize.z), color);
	DrawLine(center + float3(-hsize.x,  hsize.y, -hsize.z), center + float3(-hsize.x,  hsize.y,  hsize.z), color);
	DrawLine(center + float3( hsize.x, -hsize.y, -hsize.z), center + float3( hsize.x, -hsize.y,  hsize.z), color);
	DrawLine(center + float3( hsize.x,  hsize.y, -hsize.z), center + float3( hsize.x,  hsize.y,  hsize.z), color);
}

void DrawBox(float3 c1, float3 c2, float4 color) {
	DrawAABB((c2 + c1) / 2, c2 - c1, color);
}

void DrawPoint(float3 point, float4 color) {
	DrawLine(point - float3(0.1, 0, 0), point + float3(0.1, 0, 0), color);
	DrawLine(point - float3(0, 0.1, 0), point + float3(0, 0.1, 0), color);
	DrawLine(point - float3(0,0,  0.1), point + float3(0,0,  0.1), color);
}

void DrawRectangle(float4 color, int x, int y, int w, int h) {
	current_layer->quads.push_back(DrawQuadRecord{
		.mode = DrawQuadMode_SolidColor,
		.position_rect = float4(x, y, w, h),
		.uv_rect = {0,0,0,0},
		.tint = color,
	});
}

void DrawSprite(Sprite* sprite, int x, int y, float4 tint) {
	current_layer->quads.push_back(DrawQuadRecord{
		.mode = DrawQuadMode_Sprite,
		.texture = sprite->texture,
		.position_rect = float4(x, y, sprite->w, sprite->h),
		.uv_rect = float4(
			(float)sprite->x / (float)sprite->texture->width,
			(float)sprite->y / (float)sprite->texture->height,
			(float)sprite->w / (float)sprite->texture->width,
			(float)sprite->h / (float)sprite->texture->height),
		.tint = tint,
	});
}

void DrawText(Font* font, const char* text, int len, int x, int y, float4 color) {
	if (len < 0) len = strlen(text);

	float startx = x;
	y += font->pixel_size;;
	for (int i = 0; i < len; i++) {
		char c = text[i];
		if (c < 0) continue;

		FontGlyph& glyph = font->glyphs[c];

		if (c == '\n') {
			x = startx;
			y += font->line_height;
		}

		int xx = x + glyph.xoffs;
		int yy = y - glyph.yoffs;

		current_layer->quads.push_back(DrawQuadRecord{
			.mode = DrawQuadMode_Text,
			.texture = font->atlas,
			.position_rect = float4(xx, yy, glyph.width, glyph.height),
			.uv_rect = float4(
				(float)glyph.x / (float)font->atlas->width,
				(float)glyph.y / (float)font->atlas->height,
				(float)glyph.width / (float)font->atlas->width,
				(float)glyph.height / (float)font->atlas->height),
			.tint = color,
		});

		x += glyph.advance;
	}
}

float2 MeasureText(Font* font, const char* text, int len) {
	if (len < 0) len = strlen(text);

	float row = 0;
	float2 size = {};
	size.y = font->pixel_size;

	for (int i = 0; i < len; i++) {
		char c = text[i];
		if (c < 0) continue;

		FontGlyph& glyph = font->glyphs[c];

		if (c == '\n') {
			row = 0;
			size.y += font->line_height;
		}

		row += glyph.advance;
		if (row > size.x) size.x = row;
	}
	return size;
}
