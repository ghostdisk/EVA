#include <EVA/GPU/GPU.hpp>
#include <EVA/PAL/PAL.hpp>
#include <vector>

using namespace EVA;

PAL::Window window;
bool quit = false;

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

	GPU::Init({
		.window = &window,
		.preferred_backend = GPU::Backend::VULKAN,
	});
	DEFER(GPU::Shutdown());
	uint32 backbuffer_count = GPU::device.GetBackbufferCount();
	if (!backbuffer_count)
		return 1;
	GPU::Texture* first_backbuffer = GPU::device.GetBackbuffer(0);
	if (!first_backbuffer)
		return 1;

	GPU::RenderPass* render_pass = GPU::device.CreateRenderPass({
		.attachments = {
			GPU::AttachmentDesc{
				.format = GPU::device.GetTextureDesc(first_backbuffer).format,
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
	DEFER(
		for (BackbufferFramebuffer& entry : backbuffers)
			GPU::device.DestroyFramebuffer(entry.framebuffer));
	for (uint32 i = 0; i < backbuffer_count; ++i)
	{
		backbuffers[i].texture = GPU::device.GetBackbuffer(i);
		if (!backbuffers[i].texture)
			return 1;
		backbuffers[i].framebuffer = GPU::device.CreateFramebuffer({
			.render_pass = render_pass,
			.attachments = { backbuffers[i].texture },
		});
		if (!backbuffers[i].framebuffer)
			return 1;
	}
	int result = 0;

	while (!quit)
	{
		PAL::Event event;

		while (PAL::Poll(&event))
		{
			switch (event.type)
			{
			case PAL::EventType::CLOSE_REQUESTED:
			{
				quit = true;
				break;
			}
			default:
				break;
			}
		}
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
