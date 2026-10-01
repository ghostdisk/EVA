#include <EVA/GPU/GPU.hpp>
#include <EVA/Script/Script.hpp>
#include <EVA/PAL/PAL.hpp>
#include <vector>
#ifdef EVA_MACOS
#include <unistd.h>
#endif

using namespace EVA;

PAL::Window window;
bool quit = false;

static const char* triangle_shader_source = R"(
const positions: [3]float2 = {
	float2( 0.0,  0.5),
	float2( 0.5, -0.5),
	float2(-0.5, -0.5),
};

struct VSOutput
{
	position: float4;
}

function VSMain(@builtin(vertex_index) vertex_id: uint): @builtin(position) float4
{
	return float4(positions[vertex_id], 0.0, 1.0);
}

function PSMain(): @location(0) float4
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
)";

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
	Arena* shader_arena = CreateArena(1024 * 1024);
	DEFER(DestroyArena(shader_arena));

	Script::CompileShaderResult triangle_shader = Script::CompileShader(shader_arena, triangle_shader_source);
	if (triangle_shader.errors.count)
	{
		for (uint32 i = 0; i < triangle_shader.errors.count; ++i)
			printf("error: %s\n", triangle_shader.errors[i]->message.CString());
		return 1;
	}

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
#else
		.preferred_backend = GPU::Backend::VULKAN,
#endif
		.debug = true,
	});
	DEFER(GPU::Shutdown());
	uint32 backbuffer_count = GPU::device.GetBackbufferCount();
	if (!backbuffer_count)
	{
#ifdef EVA_MACOS
		// Keep the PAL test window responsive until Metal exposes backbuffers.
		while (!quit)
		{
			PollEvents();
			if (!quit)
				usleep(16000);
		}
		return 0;
#else
		return 1;
#endif
	}

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
		GPU::device.BeginRenderPass({
			.render_pass = render_pass,
			.framebuffer = framebuffers[GPU::device.GetCurrentBackbufferIndex()],
			.clear_values = { { .color = { 1.0f, 0.0f, 0.0f, 1.0f } } },
		});
		GPU::device.EndRenderPass();
		GPU::device.EndFrame();
	}

	return 0;
}
