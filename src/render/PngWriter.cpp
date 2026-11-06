#include "PngWriter.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <format>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace oc4::render
{
	namespace
	{
		void check(HRESULT hr, const char* what)
		{
			if (FAILED(hr))
				throw std::runtime_error(std::format("{} failed: 0x{:08X}", what, static_cast<uint32_t>(hr)));
		}
	}

	void writePng(const std::filesystem::path& path, const std::vector<uint8_t>& bgra, uint32_t width, uint32_t height)
	{
		// COM on this thread, if the caller has not (an apartment of either kind will do).
		(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		ComPtr<IWICImagingFactory> factory;
		check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "WIC factory");
		ComPtr<IWICStream> stream;
		check(factory->CreateStream(&stream), "CreateStream");
		check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "InitializeFromFilename");
		ComPtr<IWICBitmapEncoder> encoder;
		check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "CreateEncoder");
		check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "encoder Initialize");
		ComPtr<IWICBitmapFrameEncode> frame;
		check(encoder->CreateNewFrame(&frame, nullptr), "CreateNewFrame");
		check(frame->Initialize(nullptr), "frame Initialize");
		check(frame->SetSize(width, height), "SetSize");
		WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
		check(frame->SetPixelFormat(&format), "SetPixelFormat");
		if (format != GUID_WICPixelFormat32bppBGRA)
			throw std::runtime_error("PNG encoder does not accept BGRA");
		check(frame->WritePixels(height, width * 4, static_cast<UINT>(bgra.size()), const_cast<BYTE*>(bgra.data())), "WritePixels");
		check(frame->Commit(), "frame Commit");
		check(encoder->Commit(), "encoder Commit");
	}
}
