#include <EVA/GPU/GPU_D3D11.hpp>
#include <EVA/PAL/PAL.hpp>
#include <EVA/Core/Panic.hpp>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <vector>

#define HRES_ASSERT(expr)                                                                                   \
	do                                                                                                      \
	{                                                                                                       \
		HRESULT hres_assert_result = (expr);                                                                \
		if (FAILED(hres_assert_result))                                                                     \
		{                                                                                                   \
			Panic("%s failed with HRESULT 0x%08lX at %s:%d", #expr, (unsigned long)hres_assert_result,     \
				__FILE__, __LINE__);                                                                        \
		}                                                                                                   \
	} while (0)

#define D3D11_ASSERT(expr)                                                                                  \
	do                                                                                                      \
	{                                                                                                       \
		if (!(expr))                                                                                        \
			Panic("%s failed at %s:%d", #expr, __FILE__, __LINE__);                                         \
	} while (0)

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

struct D3D11Pipeline
{
	ID3D11VertexShader* vertex_shader = nullptr;
	ID3D11PixelShader* pixel_shader = nullptr; // nullptr without a fragment shader
	ID3D11RasterizerState* rasterizer_state = nullptr;
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
static bool swapchain_dirty = false;
static uint32 window_width = 0;
static uint32 window_height = 0;

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

static D3D11Pipeline* ToImpl(Pipeline* pipeline)
{
	return reinterpret_cast<D3D11Pipeline*>(pipeline);
}

static RenderPass* CreateRenderPass(const RenderPassDesc& desc)
{
	D3D11_ASSERT(desc.attachments.data && desc.attachments.count &&
		desc.attachments.count <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + 1);
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
	D3D11_ASSERT(desc.render_pass && desc.attachments.data);
	auto* render_pass = ToImpl(desc.render_pass);
	D3D11_ASSERT(desc.attachments.count == render_pass->attachments.size());
	auto* framebuffer = new D3D11Framebuffer;
	framebuffer->render_pass = render_pass;
	for (uint32 i = 0; i < desc.attachments.count; ++i)
	{
		D3D11_ASSERT(desc.attachments[i]);
		auto* texture = ToImpl(desc.attachments[i]);
		if (i == 0)
		{
			framebuffer->width = texture->desc.width;
			framebuffer->height = texture->desc.height;
			framebuffer->layers = texture->desc.layers;
		}
		if (render_pass->attachments[i].state_during == ImageState::COLOR_ATTACHMENT)
		{
			D3D11_ASSERT(framebuffer->color_count < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
			auto* view = texture->render_target_view;
			D3D11_ASSERT(view);
			view->AddRef();
			framebuffer->attachment_color_views[i] = view;
			framebuffer->color_views[framebuffer->color_count++] = view;
		}
		else
		{
			D3D11_ASSERT(!framebuffer->depth_view && texture->depth_stencil_view);
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

static uint32 GetCurrentBackbufferIndex()
{
	return 0;
}

static uint32 GetBackbufferCount()
{
	return backbuffer.render_target_view ? 1 : 0;
}

static Texture* GetBackbuffer(uint32 index)
{
	return index < GetBackbufferCount() ? reinterpret_cast<Texture*>(&backbuffer) : nullptr;
}

static TextureDesc GetTextureDesc(Texture* texture)
{
	return texture ? ToImpl(texture)->desc : TextureDesc{};
}

static bool AcquireBackbuffer()
{
	if (FAILED(d3d_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer.resource))))
		return false;

	D3D11_TEXTURE2D_DESC backbuffer_desc = {};
	backbuffer.resource->GetDesc(&backbuffer_desc);
	backbuffer.desc.width = backbuffer_desc.Width;
	backbuffer.desc.height = backbuffer_desc.Height;
	backbuffer.desc.layers = backbuffer_desc.ArraySize;
	backbuffer.desc.mip_levels = backbuffer_desc.MipLevels;
	backbuffer.desc.format = TextureFormat::RGBA8_UNORM;

	return SUCCEEDED(d3d_device->CreateRenderTargetView(backbuffer.resource, nullptr, &backbuffer.render_target_view));
}

static void ReleaseBackbuffer()
{
	if (backbuffer.render_target_view)
		backbuffer.render_target_view->Release();
	if (backbuffer.resource)
		backbuffer.resource->Release();
	backbuffer = {};
}

static bool RecreateSwapchain()
{
	if (!d3d_swapchain)
		return false;
	ReleaseBackbuffer();
	// ResizeBuffers fails while the context still holds the old backbuffer, including deferred releases.
	d3d_context->ClearState();
	d3d_context->Flush();
	HRES_ASSERT(d3d_swapchain->ResizeBuffers(0, window_width, window_height, DXGI_FORMAT_UNKNOWN, 0));
	if (!AcquireBackbuffer())
		return false;
	swapchain_dirty = false;
	return true;
}

static FrameStatus BeginFrame()
{
	if (!d3d_swapchain)
		return FrameStatus::SKIP;
	if (swapchain_dirty)
		return window_width && window_height ? FrameStatus::SWAPCHAIN_OUTDATED : FrameStatus::SKIP;
	return FrameStatus::OK;
}

static void DestroyPipeline(Pipeline* pipeline)
{
	if (!pipeline)
		return;
	auto* impl = ToImpl(pipeline);
	if (impl->vertex_shader)
		impl->vertex_shader->Release();
	if (impl->pixel_shader)
		impl->pixel_shader->Release();
	if (impl->rasterizer_state)
		impl->rasterizer_state->Release();
	delete impl;
}

// The shader's HLSL compiled by fxc, or nullptr with its errors printed. fxc fails for shaders that run out of
// registers, which the shader compiler doesn't check (Docs/Plan/Shaders.md, 8).
static ID3DBlob* CompileHLSL(const CompiledEntryPoint& shader)
{
	const char* profile = shader.stage == ShaderStage::VERTEX ? "vs_5_0" : "ps_5_0";
	ID3DBlob* code = nullptr;
	ID3DBlob* errors = nullptr;
	HRESULT result = D3DCompile(shader.code.data, shader.code.count, nullptr, nullptr, nullptr, ENTRY_POINT_NAME, profile,
		D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
	if (FAILED(result))
		fprintf(stderr, "fxc failed to compile a shader: %s\n", errors ? (const char*)errors->GetBufferPointer() : "no message");
	if (errors)
		errors->Release();
	return SUCCEEDED(result) ? code : nullptr;
}

static Pipeline* CreatePipeline(const CreatePipelineOptions& options)
{
	D3D11_ASSERT(options.render_pass);
	auto* pipeline = new D3D11Pipeline;
	for (uint32 i = 0; i < options.shaders.count; ++i)
	{
		const CompiledEntryPoint& shader = options.shaders[i];
		ID3DBlob* code = CompileHLSL(shader);
		if (!code)
		{
			DestroyPipeline(reinterpret_cast<Pipeline*>(pipeline));
			return nullptr;
		}
		DEFER(code->Release());
		if (shader.stage == ShaderStage::VERTEX)
		{
			D3D11_ASSERT(!pipeline->vertex_shader);
			HRES_ASSERT(d3d_device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
				&pipeline->vertex_shader));
		}
		else
		{
			D3D11_ASSERT(!pipeline->pixel_shader);
			HRES_ASSERT(d3d_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
				&pipeline->pixel_shader));
		}
	}
	D3D11_ASSERT(pipeline->vertex_shader);

	// Clockwise on screen is the front, like every backend's.
	D3D11_RASTERIZER_DESC rasterizer = {};
	rasterizer.FillMode = D3D11_FILL_SOLID;
	rasterizer.CullMode = D3D11_CULL_NONE;
	rasterizer.FrontCounterClockwise = FALSE;
	rasterizer.DepthClipEnable = TRUE;
	HRES_ASSERT(d3d_device->CreateRasterizerState(&rasterizer, &pipeline->rasterizer_state));
	return reinterpret_cast<Pipeline*>(pipeline);
}

static void CmdBindPipeline(Pipeline* pipeline)
{
	D3D11_ASSERT(pipeline);
	auto* impl = ToImpl(pipeline);
	d3d_context->IASetInputLayout(nullptr);
	d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3d_context->VSSetShader(impl->vertex_shader, nullptr, 0);
	d3d_context->PSSetShader(impl->pixel_shader, nullptr, 0);
	d3d_context->RSSetState(impl->rasterizer_state);
}

static void CmdDraw(uint32 vertex_count, uint32 first_vertex)
{
	d3d_context->Draw(vertex_count, first_vertex);
}

static void CmdBeginRenderPass(const RenderPassBeginDesc& desc)
{
	D3D11_ASSERT(desc.render_pass && desc.framebuffer);
	auto* render_pass = ToImpl(desc.render_pass);
	auto* framebuffer = ToImpl(desc.framebuffer);
	D3D11_ASSERT(framebuffer->render_pass == render_pass &&
		(!desc.clear_values.count || desc.clear_values.count == render_pass->attachments.size()));
	for (uint32 i = 0; i < render_pass->attachments.size(); ++i)
	{
		D3D11_ASSERT(render_pass->attachments[i].load_op != AttachmentLoadOp::CLEAR ||
			(desc.clear_values.data && i < desc.clear_values.count));
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

static void CmdEndRenderPass()
{
	d3d_context->OMSetRenderTargets(0, nullptr, nullptr);
}

static void EndFrame()
{
	HRES_ASSERT(d3d_swapchain->Present(1, 0));
}

static void Shutdown()
{
	if (d3d_context)
	{
		d3d_context->ClearState();
	}
	ReleaseBackbuffer();
	swapchain_dirty = false;
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

static void HandlePALEvent(const PAL::Event& event)
{
	switch (event.type)
	{
	case PAL::EventType::WINDOW_RESIZE:
		// The flip model stretches instead of reporting an outdated swapchain, so track the size ourselves.
		window_width = (uint32)event.width;
		window_height = (uint32)event.height;
		swapchain_dirty = window_width != backbuffer.desc.width || window_height != backbuffer.desc.height;
		break;
	default:
		break;
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

	if (!AcquireBackbuffer())
		return false;
	window_width = backbuffer.desc.width;
	window_height = backbuffer.desc.height;
	return true;
}

static bool Init(Device& out_device, const InitOptions& init_options)
{
	if (!InitImpl(out_device, init_options))
	{
		Shutdown();
		return false;
	}

	out_device = Device{
		.backend = Backend::D3D11,
		.backbuffer_format = backbuffer.desc.format,
		.Shutdown = Shutdown,
		.HandlePALEvent = HandlePALEvent,
		.CreateRenderPass = CreateRenderPass,
		.DestroyRenderPass = DestroyRenderPass,
		.CreateFramebuffer = CreateFramebuffer,
		.DestroyFramebuffer = DestroyFramebuffer,
		.GetBackbufferCount = GetBackbufferCount,
		.GetBackbuffer = GetBackbuffer,
		.GetTextureDesc = GetTextureDesc,
		.RecreateSwapchain = RecreateSwapchain,
		.BeginFrame = BeginFrame,
		.GetCurrentBackbufferIndex = GetCurrentBackbufferIndex,
		.CreatePipeline = CreatePipeline,
		.DestroyPipeline = DestroyPipeline,
		.CmdBeginRenderPass = CmdBeginRenderPass,
		.CmdEndRenderPass = CmdEndRenderPass,
		.CmdBindPipeline = CmdBindPipeline,
		.CmdDraw = CmdDraw,
		.EndFrame = EndFrame,
	};
	return true;
}

BackendDesc backend_desc = {
	.backend = Backend::D3D11,
	.Init = Init,
};

}
