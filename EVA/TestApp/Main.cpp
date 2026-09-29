#include <EVA/GPU/GPU.hpp>
#include <EVA/PAL/PAL.hpp>
#include <vector>
#ifdef EVA_MACOS
#include <unistd.h>
#endif

using namespace EVA;

PAL::Window window;
bool quit = false;

static void PollEvents()
{
	PAL::Event event;
	while (PAL::Poll(&event))
	{
		if (event.type == PAL::EventType::CLOSE_REQUESTED || event.type == PAL::EventType::QUIT_REQUESTED)
			quit = true;
	}
}

struct BackbufferFramebuffer
{
	GPU::Texture* texture = nullptr;
	GPU::Framebuffer* framebuffer = nullptr;
};

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
#else
		.preferred_backend = GPU::Backend::VULKAN,
#endif
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
				.state_before = GPU::ImageState::PRESENT,
				.state_during = GPU::ImageState::COLOR_ATTACHMENT,
				.state_after = GPU::ImageState::PRESENT,
			},
		},
	});
	if (!render_pass)
		return 1;

	DEFER(GPU::device.DestroyRenderPass(render_pass));
	std::vector<BackbufferFramebuffer> backbuffers(backbuffer_count);
	DEFER({
		for (BackbufferFramebuffer& entry : backbuffers)
			GPU::device.DestroyFramebuffer(entry.framebuffer);
	});
	for (uint32 i = 0; i < backbuffer_count; ++i)
	{
		backbuffers[i].texture = GPU::device.GetBackbuffer(i);
		backbuffers[i].framebuffer = GPU::device.CreateFramebuffer({
			.render_pass = render_pass,
			.attachments = { backbuffers[i].texture },
		});
	}
	int result = 0;

	while (!quit)
	{
		PollEvents();
		if (quit)
			break;

		if (!GPU::device.BeginFrame())
			continue;
		GPU::Texture* backbuffer = GPU::device.GetCurrentBackbuffer();
		if (!backbuffer)
		{
			result = 1;
			break;
		}
		BackbufferFramebuffer* entry = nullptr;
		for (uint32 i = 0; i < backbuffer_count; ++i)
		{
			if (backbuffers[i].texture == backbuffer)
			{
				entry = &backbuffers[i];
				break;
			}
		}
		if (!entry)
		{
			result = 1;
			break;
		}
		GPU::Framebuffer* framebuffer = entry->framebuffer;
		GPU::device.BeginRenderPass({
			.render_pass = render_pass,
			.framebuffer = framebuffer,
			.clear_values = { { .color = { 1.0f, 0.0f, 0.0f, 1.0f } } },
		});
		GPU::device.EndRenderPass();
		if (!GPU::device.EndFrame())
		{
			result = 1;
			break;
		}
	}

	return result;
}
