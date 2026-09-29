#include <EVA/GPU/GPU_D3D11.hpp>
#include <EVA/PAL/PAL.hpp>
#include <d3d11.h>
#include <dxgi1_2.h>

namespace EVA::GPU::D3D11
{

static ID3D11Device* d3d_device = nullptr;
static ID3D11DeviceContext* d3d_context = nullptr;
static IDXGISwapChain1* d3d_swapchain = nullptr;

static void Shutdown()
{
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
	};
	return true;
}

BackendDesc backend_desc = {
	.backend = Backend::D3D11,
	.Init = Init,
};

}
