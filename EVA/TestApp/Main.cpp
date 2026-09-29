#include <EVA/GPU/GPU.hpp>
#include <EVA/PAL/PAL.hpp>

using namespace EVA;

PAL::Window window;
bool quit = false;

int main()
{
	PAL::Init();

	PAL::InitWindow(&window,
		{
			.name = "EVA Test App",
			.width = 800,
			.height = 600,
		});
	DEFER(PAL::DeinitWindow(&window));

	GPU::Init({
		.window = &window,
		.preferred_backend = GPU::Backend::D3D11,
	});
	DEFER(GPU::Shutdown());

	GPU::RenderPass* render_pass = GPU::device.CreateRenderPass({
		.attachments = {
			GPU::AttachmentDesc{
				.format = GPU::TextureFormat::RGBA8_UNORM,
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

		GPU::Framebuffer* framebuffer = GPU::device.CreateFramebuffer({
			.render_pass = render_pass,
			.attachments = { GPU::device.GetCurrentBackbuffer() },
		});
		if (!framebuffer)
		{
			result = 1;
			break;
		}
		GPU::device.BeginRenderPass({
			.render_pass = render_pass,
			.framebuffer = framebuffer,
			.clear_values = { { .color = { 1.0f, 0.0f, 0.0f, 1.0f } } },
		});
		GPU::device.EndRenderPass();
		GPU::device.DestroyFramebuffer(framebuffer);
		if (!GPU::device.Present())
		{
			result = 1;
			break;
		}
	}

	return result;
}
