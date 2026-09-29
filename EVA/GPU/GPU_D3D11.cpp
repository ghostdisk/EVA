#include <EVA/GPU/GPU_D3D11.hpp>
#include <EVA/PAL/PAL.hpp>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cassert>
#include <vector>

namespace EVA::GPU::D3D11
{

struct D3D11Texture
{
	ID3D11Texture2D* resource = nullptr;
	ID3D11RenderTargetView* render_target_view = nullptr;
	ID3D11DepthStencilView* depth_stencil_view = nullptr;
	TextureDesc desc;
};

struct D3D11RenderPass
{
	std::vector<AttachmentDesc> attachments;
};

struct D3D11Framebuffer
{
	D3D11RenderPass* render_pass = nullptr;
	ID3D11RenderTargetView* color_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
	ID3D11RenderTargetView* attachment_color_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + 1] = {};
	ID3D11DepthStencilView* depth_view = nullptr;
	uint32 color_count = 0;
	uint32 width = 0;
	uint32 height = 0;
	uint32 layers = 1;
};

static ID3D11Device* d3d_device = nullptr;
static ID3D11DeviceContext* d3d_context = nullptr;
static IDXGISwapChain1* d3d_swapchain = nullptr;
static D3D11Texture backbuffer;

static D3D11RenderPass* ToImpl(RenderPass* render_pass)
{
	return reinterpret_cast<D3D11RenderPass*>(render_pass);
}

static D3D11Framebuffer* ToImpl(Framebuffer* framebuffer)
{
	return reinterpret_cast<D3D11Framebuffer*>(framebuffer);
}

static D3D11Texture* ToImpl(Texture* texture)
{
	return reinterpret_cast<D3D11Texture*>(texture);
}

static RenderPass* CreateRenderPass(const RenderPassDesc& desc)
{
	if (!desc.attachments.data || !desc.attachments.count || desc.attachments.count > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + 1)
		return nullptr;

	uint32 color_count = 0;
	uint32 depth_count = 0;
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		const AttachmentDesc& attachment = desc.attachments[i];
		if (attachment.load_op == AttachmentLoadOp::LOAD && attachment.state_before == ImageState::UNDEFINED)
			return nullptr;
		if (attachment.state_after == ImageState::UNDEFINED)
			return nullptr;
		if (attachment.format == TextureFormat::RGBA8_UNORM && attachment.state_during == ImageState::COLOR_ATTACHMENT)
			++color_count;
		else if (attachment.format == TextureFormat::D24_UNORM_S8_UINT && attachment.state_during == ImageState::DEPTH_STENCIL_ATTACHMENT)
			++depth_count;
		else
			return nullptr;
	}
	if (color_count > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT || depth_count > 1)
		return nullptr;

	auto* render_pass = new D3D11RenderPass;
	render_pass->attachments.assign(desc.attachments.data, desc.attachments.data + desc.attachments.count);
	return reinterpret_cast<RenderPass*>(render_pass);
}

static void DestroyRenderPass(RenderPass* render_pass)
{
	if (!render_pass)
		return;
	auto* impl = ToImpl(render_pass);
	delete impl;
}

static Framebuffer* CreateFramebuffer(FramebufferDesc&& desc)
{
	if (!desc.render_pass || !desc.attachments.data)
		return nullptr;
	auto* render_pass = ToImpl(desc.render_pass);
	if (desc.attachments.count != render_pass->attachments.size())
		return nullptr;

	uint32 width = 0;
	uint32 height = 0;
	uint32 layers = 0;
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		if (!desc.attachments[i])
			return nullptr;
		auto* texture = ToImpl(desc.attachments[i]);
		if (!texture->resource || texture->desc.format != render_pass->attachments[i].format)
			return nullptr;
		if (i == 0)
		{
			width = texture->desc.width;
			height = texture->desc.height;
			layers = texture->desc.layers;
		}
		if (!width || !height || !layers || texture->desc.width != width || texture->desc.height != height || texture->desc.layers != layers)
			return nullptr;
		if (render_pass->attachments[i].state_during == ImageState::COLOR_ATTACHMENT && !texture->render_target_view)
			return nullptr;
		if (render_pass->attachments[i].state_during == ImageState::DEPTH_STENCIL_ATTACHMENT && !texture->depth_stencil_view)
			return nullptr;
	}

	auto* framebuffer = new D3D11Framebuffer;
	framebuffer->render_pass = render_pass;
	framebuffer->width = width;
	framebuffer->height = height;
	framebuffer->layers = layers;
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		auto* texture = ToImpl(desc.attachments[i]);
		if (render_pass->attachments[i].state_during == ImageState::COLOR_ATTACHMENT)
		{
			auto* view = texture->render_target_view;
			view->AddRef();
			framebuffer->attachment_color_views[i] = view;
			framebuffer->color_views[framebuffer->color_count++] = view;
		}
		else
		{
			framebuffer->depth_view = texture->depth_stencil_view;
			framebuffer->depth_view->AddRef();
		}
	}
	return reinterpret_cast<Framebuffer*>(framebuffer);
}

static void DestroyFramebuffer(Framebuffer* framebuffer)
{
	if (!framebuffer)
		return;
	auto* impl = ToImpl(framebuffer);
	for (auto* view : impl->color_views)
	{
		if (view)
			view->Release();
	}
	if (impl->depth_view)
		impl->depth_view->Release();
	delete impl;
}

static Texture* GetCurrentBackbuffer()
{
	return d3d_swapchain ? reinterpret_cast<Texture*>(&backbuffer) : nullptr;
}

static uint32 GetBackbufferCount()
{
	return d3d_swapchain ? 1 : 0;
}

static Texture* GetBackbuffer(uint32 index)
{
	return index == 0 ? GetCurrentBackbuffer() : nullptr;
}

static TextureDesc GetTextureDesc(Texture* texture)
{
	return texture ? ToImpl(texture)->desc : TextureDesc{};
}

static bool BeginFrame()
{
	return d3d_swapchain != nullptr;
}

static void BeginRenderPass(const RenderPassBeginDesc& desc)
{
	if (!desc.render_pass || !desc.framebuffer)
	{
		assert(false);
		return;
	}
	auto* render_pass = ToImpl(desc.render_pass);
	auto* framebuffer = ToImpl(desc.framebuffer);
	if (framebuffer->render_pass != render_pass ||
		(desc.clear_values.count && desc.clear_values.count != render_pass->attachments.size()))
	{
		assert(false);
		return;
	}
	for (uint32 i = 0; i < render_pass->attachments.size(); ++i)
	{
		if (render_pass->attachments[i].load_op == AttachmentLoadOp::CLEAR &&
			(!desc.clear_values.data || i >= desc.clear_values.count))
		{
			assert(false);
			return;
		}
	}

	d3d_context->OMSetRenderTargets(framebuffer->color_count, framebuffer->color_views, framebuffer->depth_view);
	D3D11_VIEWPORT viewport = {};
	viewport.Width = (float)framebuffer->width;
	viewport.Height = (float)framebuffer->height;
	viewport.MaxDepth = 1.0f;
	d3d_context->RSSetViewports(1, &viewport);

	for (uint32 i = 0; i < render_pass->attachments.size(); ++i)
	{
		if (render_pass->attachments[i].load_op != AttachmentLoadOp::CLEAR)
			continue;
		if (render_pass->attachments[i].state_during == ImageState::COLOR_ATTACHMENT)
			d3d_context->ClearRenderTargetView(framebuffer->attachment_color_views[i], desc.clear_values[i].color);
		else
			d3d_context->ClearDepthStencilView(framebuffer->depth_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
				desc.clear_values[i].depth, desc.clear_values[i].stencil);
	}
}

static void EndRenderPass()
{
	d3d_context->OMSetRenderTargets(0, nullptr, nullptr);
}

static bool EndFrame()
{
	return SUCCEEDED(d3d_swapchain->Present(1, 0));
}

static void Shutdown()
{
	if (d3d_context)
	{
		d3d_context->ClearState();
	}
	if (backbuffer.render_target_view)
	{
		backbuffer.render_target_view->Release();
		backbuffer.render_target_view = nullptr;
	}
	if (backbuffer.resource)
	{
		backbuffer.resource->Release();
		backbuffer.resource = nullptr;
	}
	backbuffer = {};
	if (d3d_swapchain)
	{
		d3d_swapchain->Release();
		d3d_swapchain = nullptr;
	}
	if (d3d_context)
	{
		d3d_context->Release();
		d3d_context = nullptr;
	}
	if (d3d_device)
	{
		d3d_device->Release();
		d3d_device = nullptr;
	}
}

static bool InitImpl(Device& out_device, const InitOptions& init_options)
{
	(void)out_device;
	if (!init_options.window || !init_options.window->native_handle)
		return false;

	HRESULT result = D3D11CreateDevice(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		nullptr, 0, D3D11_SDK_VERSION,
		&d3d_device, nullptr, &d3d_context);
	if (FAILED(result))
		return false;

	IDXGIDevice* dxgi_device = nullptr;
	result = d3d_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
	if (FAILED(result))
		return false;
	DEFER(dxgi_device->Release());

	IDXGIAdapter* adapter = nullptr;
	result = dxgi_device->GetAdapter(&adapter);
	if (FAILED(result))
		return false;
	DEFER(adapter->Release());

	IDXGIFactory2* factory = nullptr;
	result = adapter->GetParent(IID_PPV_ARGS(&factory));
	if (FAILED(result))
		return false;
	DEFER(factory->Release());

	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
	swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_desc.SampleDesc.Count = 1;
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.BufferCount = 2;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	result = factory->CreateSwapChainForHwnd(
		d3d_device, static_cast<HWND>(init_options.window->native_handle),
		&swapchain_desc, nullptr, nullptr, &d3d_swapchain);
	if (FAILED(result))
		return false;

	result = d3d_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer.resource));
	if (FAILED(result))
		return false;

	D3D11_TEXTURE2D_DESC backbuffer_desc = {};
	backbuffer.resource->GetDesc(&backbuffer_desc);
	backbuffer.desc.width = backbuffer_desc.Width;
	backbuffer.desc.height = backbuffer_desc.Height;
	backbuffer.desc.layers = backbuffer_desc.ArraySize;
	backbuffer.desc.mip_levels = backbuffer_desc.MipLevels;
	backbuffer.desc.format = TextureFormat::RGBA8_UNORM;

	result = d3d_device->CreateRenderTargetView(backbuffer.resource, nullptr, &backbuffer.render_target_view);
	return SUCCEEDED(result);
}

static bool Init(Device& out_device, const InitOptions& init_options)
{
	if (!InitImpl(out_device, init_options))
	{
		Shutdown();
		return false;
	}

	out_device = Device{
		.Shutdown = Shutdown,
		.CreateRenderPass = CreateRenderPass,
		.DestroyRenderPass = DestroyRenderPass,
		.CreateFramebuffer = CreateFramebuffer,
		.DestroyFramebuffer = DestroyFramebuffer,
		.GetBackbufferCount = GetBackbufferCount,
		.GetBackbuffer = GetBackbuffer,
		.GetTextureDesc = GetTextureDesc,
		.BeginFrame = BeginFrame,
		.GetCurrentBackbuffer = GetCurrentBackbuffer,
		.BeginRenderPass = BeginRenderPass,
		.EndRenderPass = EndRenderPass,
		.EndFrame = EndFrame,
	};
	return true;
}

BackendDesc backend_desc = {
	.backend = Backend::D3D11,
	.Init = Init,
};

}
