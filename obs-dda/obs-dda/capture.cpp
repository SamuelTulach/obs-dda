#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <obs.h>
#include "capture.h"

ID3D11Device* capture_device;
ID3D11DeviceContext* capture_context;

HDESK capture_desktop;

IDXGIDevice* capture_dxgi;
IDXGIAdapter* capture_adapter;
IDXGIOutput* capture_output;
int capture_display_number = 0;

IDXGIOutput1* capture_first_output;
IDXGIOutputDuplication* capture_duplication;

extern "C" int capture_init()
{
	D3D_FEATURE_LEVEL feature_level;
	const D3D_FEATURE_LEVEL feature_level_array[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

	if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, feature_level_array, 2, D3D11_SDK_VERSION, &capture_device, &feature_level, &capture_context) != S_OK)
		return false;

	capture_desktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
	if (!capture_desktop)
		return false;

	/*const auto attached = SetThreadDesktop(capture_desktop);
	CloseDesktop(capture_desktop);
	capture_desktop = NULL;
	auto test = GetLastError();
	if (!attached)
		return false;*/

	HRESULT status = capture_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&capture_dxgi);
	if (FAILED(status))
		return false;

	status = capture_dxgi->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&capture_adapter));
	if (FAILED(status))
		return false;

	status = capture_adapter->EnumOutputs(capture_display_number, &capture_output);
	if (FAILED(status))
		return false;

	status = capture_output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&capture_first_output));
	if (FAILED(status))
		return false;

	status = capture_first_output->DuplicateOutput(capture_device, &capture_duplication);
	if (FAILED(status))
		return false;

	return true;
}

static capture_bitmap current_bitmap;
static char* current_buffer;
static bool lock = false;

extern "C" capture_bitmap* capture_get_frame()
{
	// TODO: multiple frees missing in this function
	
	if (!capture_duplication)
		return &current_bitmap;

	if (lock)
	{
		lock = false;
		capture_duplication->ReleaseFrame();
	}

	IDXGIResource* resource = nullptr;
	DXGI_OUTDUPL_FRAME_INFO info;
	auto status = capture_duplication->AcquireNextFrame(0, &info, &resource);
	if (FAILED(status))
		return &current_bitmap;

	lock = true;

	ID3D11Texture2D* gpu_texture = nullptr;
	status = resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&gpu_texture));
	resource->Release();
	resource = nullptr;
	if (FAILED(status))
		return &current_bitmap;

	D3D11_TEXTURE2D_DESC descriptor;
	gpu_texture->GetDesc(&descriptor);

	descriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
	descriptor.Usage = D3D11_USAGE_STAGING;
	descriptor.BindFlags = 0;
	descriptor.MiscFlags = 0;

	ID3D11Texture2D* cpu_texture = nullptr;
	status = capture_device->CreateTexture2D(&descriptor, nullptr, &cpu_texture);
	if (FAILED(status))
		return &current_bitmap;

	capture_context->CopyResource(cpu_texture, gpu_texture);

	D3D11_MAPPED_SUBRESOURCE subResource;
	status = capture_context->Map(cpu_texture, 0, D3D11_MAP_READ, 0, &subResource);
	if (FAILED(status))
		return &current_bitmap;

	if (current_bitmap.width != descriptor.Width || current_bitmap.height != descriptor.Height)
	{
		current_bitmap.width = descriptor.Width;
		current_bitmap.height = descriptor.Height;

		if (!current_buffer)
		{
			free(current_buffer);
		}

		current_buffer = (char*)malloc((uint64_t)descriptor.Width * (uint64_t)descriptor.Height * 4);
	}

	if (!current_buffer)
		return &current_bitmap;

	for (auto y = 0; y < descriptor.Height; y++)
		memcpy(current_buffer + y * (uint64_t)descriptor.Width * 4, (uint8_t*)(subResource.pData) + (uint64_t)subResource.RowPitch * y, (uint64_t)descriptor.Width * 4);

	capture_context->Unmap(cpu_texture, 0);

	cpu_texture->Release();
	gpu_texture->Release();

	current_bitmap.buffer = current_buffer;

	return &current_bitmap;
}