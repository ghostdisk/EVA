#include <EVA/GPU/GPU.hpp>
#include <EVA/Script/Script.hpp>
#include <EVA/PAL/PAL.hpp>
#include <math.h>
#include <vector>

using namespace EVA;

PAL::Window window;
bool quit = false;

static const char* triangle_shader_source = R"(
const positions: [3]float2 = {
	float2( 0.0,  0.5),
	float2( 0.5, -0.5),
	float2(-0.5, -0.5),
};

@entry(vertex)
function VSMain(@semantic(vertex_index) vertex_id: uint): @semantic(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

@entry(fragment)
function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
)";

// The triangle rotated by a transform from a bind group, which only D3D11 supports so far.
static const char* rotating_triangle_shader_source = R"(
const positions: [3]float2 = {
	float2( 0.0,  0.5),
	float2( 0.5, -0.5),
	float2(-0.5, -0.5),
};

struct Transform
{
	world: float4x4;
}

@bind_group(0) let transform: Transform;

@entry(vertex)
function VSMain(@semantic(vertex_index) vertex_id: uint): @semantic(position) float4
{
	return mul(transform.world, float4(positions[vertex_id], 0.0, 1.0));
}

@entry(fragment)
function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
)";

// A rotation about z by angle, with x scaled by aspect so the triangle keeps its shape. Column-major.
static void RotationZ(float angle, float aspect, float out[16])
{
	float c = cosf(angle);
	float s = sinf(angle);
	float m[16] = {
		c * aspect, s, 0.0f, 0.0f,
		-s * aspect, c, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	for (int i = 0; i < 16; ++i)
		out[i] = m[i];
}

static std::vector<GPU::Framebuffer*> framebuffers;

static void DestroyFramebuffers()
{
	for (GPU::Framebuffer* framebuffer : framebuffers)
		GPU::device.DestroyFramebuffer(framebuffer);
	framebuffers.clear();
}

static void CreateFramebuffers(GPU::RenderPass* render_pass)
{
	uint32 count = GPU::device.GetBackbufferCount();
	framebuffers.resize(count);
	for (uint32 i = 0; i < count; ++i)
	{
		framebuffers[i] = GPU::device.CreateFramebuffer({
			.render_pass = render_pass,
			.attachments = { GPU::device.GetBackbuffer(i) },
		});
	}
}

static void PollEvents()
{
	PAL::Event event;
	while (PAL::Poll(&event))
	{
		GPU::device.HandlePALEvent(event);
		if (event.type == PAL::EventType::QUIT)
		{
			quit = true;
			break;
		}
	}
}

int EVA::AppMain()
{
	PAL::InitWindow(&window,
		{
			.name = "EVA Test App",
			.width = 800,
			.height = 600,
		});
	DEFER(PAL::DeinitWindow(&window));
	if (!window.native_handle)
		return 1;

	GPU::Init({
		.window = &window,
#ifdef EVA_MACOS
		.preferred_backend = GPU::Backend::METAL,
#elif defined(EVA_WIN32)
		.preferred_backend = GPU::Backend::D3D11, // the only backend with bind groups so far
#else
		.preferred_backend = GPU::Backend::VULKAN,
#endif
		.debug = true,
	});
	DEFER(GPU::Shutdown());
	if (!GPU::device.GetBackbufferCount())
		return 1;

	GPU::RenderPass* render_pass = GPU::device.CreateRenderPass({
		.attachments = {
			GPU::AttachmentDesc{
				.format = GPU::device.backbuffer_format,
				.load_op = GPU::AttachmentLoadOp::CLEAR,
				.state_before = GPU::ImageState::UNDEFINED,
				.state_during = GPU::ImageState::COLOR_ATTACHMENT,
				.state_after = GPU::ImageState::PRESENT,
			},
		},
	});
	if (!render_pass)
		return 1;

	DEFER(GPU::device.DestroyRenderPass(render_pass));
	DEFER(DestroyFramebuffers());
	CreateFramebuffers(render_pass);

	// Compiled for whichever backend the device got. Without a pipeline, frames are just cleared.
	Arena* shader_arena = CreateArena();
	DEFER(DestroyArena(shader_arena));
	Script::CompileShaderResult triangle_shader = Script::CompileShader({
		.arena = shader_arena,
		.source = GPU::device.CreateBindGroup ? rotating_triangle_shader_source : triangle_shader_source,
		.backend = GPU::device.backend,
	});
	for (uint32 i = 0; i < triangle_shader.errors.count; ++i)
		printf("error: %s\n", triangle_shader.errors[i]->message.CString());
	GPU::Pipeline* pipeline = nullptr;
	if (!triangle_shader.errors.count)
	{
		pipeline = GPU::device.CreatePipeline({
			.shaders = triangle_shader.entry_points,
			.render_pass = render_pass,
			.bind_groups = triangle_shader.bind_groups,
		});
	}
	DEFER(GPU::device.DestroyPipeline(pipeline));

	GPU::BindGroup* transform = nullptr;
	if (pipeline && triangle_shader.bind_groups.count)
		transform = GPU::device.CreateBindGroup(triangle_shader.bind_groups[0]);
	DEFER(if (transform) GPU::device.DestroyBindGroup(transform));
	GPU::ShaderCursor world = GPU::GetCursor(transform).Field("world");

	uint32 frame = 0;
	while (!quit)
	{
		PollEvents();
		if (quit)
			break;

		GPU::FrameStatus status = GPU::device.BeginFrame();
		if (status == GPU::FrameStatus::SKIP)
			continue;
		if (status == GPU::FrameStatus::SWAPCHAIN_OUTDATED)
		{
			DestroyFramebuffers();
			if (GPU::device.RecreateSwapchain())
				CreateFramebuffers(render_pass);
			continue;
		}
		GPU::device.CmdBeginRenderPass({
			.render_pass = render_pass,
			.framebuffer = framebuffers[GPU::device.GetCurrentBackbufferIndex()],
			.clear_values = { { .color = { 1.0f, 0.0f, 0.0f, 1.0f } } },
		});
		if (pipeline)
		{
			GPU::device.CmdBindPipeline(pipeline);
			if (transform)
			{
				GPU::TextureDesc backbuffer = GPU::device.GetTextureDesc(GPU::device.GetBackbuffer(0));
				float matrix[16];
				RotationZ((float)frame * 0.02f, (float)backbuffer.height / (float)backbuffer.width, matrix);
				world.Write(matrix, sizeof(matrix));
				GPU::device.CmdSetBindGroup(transform);
			}
			GPU::device.CmdDraw(3, 0);
		}
		frame++;
		GPU::device.CmdEndRenderPass();
		GPU::device.EndFrame();
	}

	return 0;
}
