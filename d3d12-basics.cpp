#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#pragma comment (lib, "user32.lib")
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")



#define DEBUGLOG(fmt, ...) fprintf(debug_log_file, fmt "\n", ##__VA_ARGS__)
#define ASSERT(expr) assert(expr)



static wchar_t const* window_title = L"d3d12-basics";
static int window_width = 720;
static int window_height = 480;
static float window_aspect = (float)window_width / (float)window_height;
static bool window_resized = false;

static FILE* debug_log_file;

#define BUFFER_COUNT 2
struct d3d12_data_t {
	// d3d12 data
	ID3D12Device* device;
	ID3D12CommandQueue* cmd_queue;
	IDXGISwapChain3* swapchain;
	ID3D12RootSignature* signature;
	UINT table_slot;
	UINT consts_slot;
	ID3D12PipelineState* pipeline;
	ID3D12CommandAllocator* cmd_alloc;
	ID3D12GraphicsCommandList* cmd_list;
	ID3D12Resource* upload_buffer;
	SIZE_T vertex_offset;
	SIZE_T texture_offset;
	ID3D12Resource* vertex_resource;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	ID3D12Resource* texture_resource;
	ID3D12DescriptorHeap* srv_heap;
	ID3D12Fence* fence;
	UINT64 fence_val;
	HANDLE fence_event;

	ID3D12Resource* render_targets[BUFFER_COUNT];
	ID3D12DescriptorHeap* rtv_heap;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_base;
	UINT rtv_stride;
} d3d12_data;



typedef struct vertex_t {
	float pos[2];
	float uv[2];
	float col[4];
} vertex_t;

static vertex_t vertex_data[] = {
	// position        // UV        // color
	{{+0.000f,+0.500f+0.3f}, {1.0f,1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
	{{-0.707f,-0.707f+0.3f}, {1.0f,0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
	{{+0.707f,-0.707f+0.3f}, {0.0f,1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
};

static uint32_t texture_data[] = {
	0x88000000, 0xaaffffff,
	0xaaffffff, 0x88000000,
};
static size_t texture_size[2] = { 2, 2 };

static float clear_color[] = { 0.117f, 0.3f, 0.4f, 1.0f };
static bool vsync_enabled = true;

static double win32_perf_freq;
static LARGE_INTEGER win32_perf_counter_start;



// function forward declarations
void d3d12_init(HWND widow);
void d3d12_shutdown();



double get_time_sec() {
	LARGE_INTEGER perf_counter;
	QueryPerformanceCounter(&perf_counter);
	return (double)(perf_counter.QuadPart - win32_perf_counter_start.QuadPart) / win32_perf_freq;
}


static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
	LRESULT lr = 0;

	switch (message) {
	case WM_SIZE: {
		window_width = LOWORD(lp);
		window_height = HIWORD(lp);
		window_aspect = (float)window_width / (float)window_height;
		DEBUGLOG("window resized to %i %i", window_width, window_height);
		window_resized = true;
	} break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	} break;

	default: {
		lr = DefWindowProcW(window, message, wp, lp);
	} break;
	}

	return lr;
}






// main
int WINAPI WinMain(HINSTANCE instance, HINSTANCE instance_p, LPSTR cmd_line, int cmd_show) {
	debug_log_file = fopen("log.txt", "w");

	HRESULT hresult; // we can just reuse this

	// create window
	HWND window_handle;
	{
		WNDCLASSEXW wc = {0};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.lpfnWndProc = window_proc;
		wc.hInstance = instance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.lpszClassName = window_title;

		if (!RegisterClassExW(&wc)) {
			ASSERT(0);
		}

		DWORD style = WS_OVERLAPPEDWINDOW;
		DWORD style_ex = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;

		window_handle = CreateWindowExW(
			style_ex,
			wc.lpszClassName,
			window_title,
			style,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			window_width,
			window_height,
			NULL,
			NULL,
			wc.hInstance,
			NULL
		);

		if (!window_handle) {
			ASSERT(0);
		}

	}
	DEBUGLOG("crated window");

	// initialize high-resolution timer
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&win32_perf_counter_start);
		win32_perf_freq = (double)freq.QuadPart;
	}


	// initialize Direct3D 12
	d3d12_init(window_handle);
	
	ShowWindow(window_handle, cmd_show);



	// program loop

	// to create render targets when we start
	window_resized = true;

	LARGE_INTEGER tick_0;
	LARGE_INTEGER freq;
	QueryPerformanceCounter(&tick_0);
	QueryPerformanceFrequency(&freq);

	LARGE_INTEGER tick_p;
	LARGE_INTEGER tick_n;
	tick_p.QuadPart = tick_0.QuadPart;
	tick_n.QuadPart = tick_0.QuadPart + freq.QuadPart;

	double uptime = 0.0;

	bool first_time = true;



	DEBUGLOG("start program loop");

	double last_time = 0.0;
	while (1) {
		MSG msg;
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			switch (msg.message) {
				case WM_QUIT: {
					goto main_loop_end;
				} break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		uptime = get_time_sec();

		//DEBUGLOG("frame\n");

		// create the render targets
		if (window_resized) {
			window_resized = false;

			if (d3d12_data.rtv_heap) {
				d3d12_data.cmd_list->ClearState(NULL);
				for (UINT i = 0; i < BUFFER_COUNT; i++) {
					d3d12_data.render_targets[i]->Release();
				}
				d3d12_data.rtv_heap->Release();
			}

			hresult = d3d12_data.swapchain->ResizeBuffers(
				BUFFER_COUNT,
				window_width,
				window_height,
				DXGI_FORMAT_UNKNOWN,
				0
			);
			ASSERT(SUCCEEDED(hresult));

			D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {0};
			rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtv_heap_desc.NumDescriptors = BUFFER_COUNT;

			hresult = d3d12_data.device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&d3d12_data.rtv_heap));
			ASSERT(SUCCEEDED(hresult));

			d3d12_data.rtv_base = d3d12_data.rtv_heap->GetCPUDescriptorHandleForHeapStart();
			d3d12_data.rtv_stride = d3d12_data.device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV
			);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d12_data.rtv_base;
			for (UINT i = 0; i < BUFFER_COUNT; i++) {
				hresult = d3d12_data.swapchain->GetBuffer(i, IID_PPV_ARGS(&d3d12_data.render_targets[i]));
				ASSERT(SUCCEEDED(hresult));

				d3d12_data.device->CreateRenderTargetView(d3d12_data.render_targets[i], NULL, rtv_handle);
				rtv_handle.ptr += d3d12_data.rtv_stride;
			}
		}

		// fill command lists
		{
			hresult = d3d12_data.cmd_alloc->Reset();
			ASSERT(SUCCEEDED(hresult));

			hresult = d3d12_data.cmd_list->Reset(d3d12_data.cmd_alloc, d3d12_data.pipeline);
			ASSERT(SUCCEEDED(hresult));

			if (first_time) {
				first_time = false;

				d3d12_data.cmd_list->CopyBufferRegion(
					d3d12_data.vertex_resource,
					0,
					d3d12_data.upload_buffer,
					d3d12_data.vertex_offset,
					sizeof(vertex_data)
				);

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {0};

				d3d12_data.device->GetCopyableFootprints(
					&d3d12_data.texture_resource->GetDesc(),
					0,
					1,
					d3d12_data.texture_offset,
					&footprint,
					NULL,
					NULL,
					NULL
				);

				D3D12_TEXTURE_COPY_LOCATION src = {0};
				src.pResource = d3d12_data.upload_buffer;
				src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				src.PlacedFootprint = footprint;

				D3D12_TEXTURE_COPY_LOCATION dst = {0};
				dst.pResource = d3d12_data.texture_resource;
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = 0;

				d3d12_data.cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);



				D3D12_RESOURCE_BARRIER vb = {0};
				vb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				vb.Transition.pResource = d3d12_data.vertex_resource;
				vb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				vb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				vb.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

				D3D12_RESOURCE_BARRIER tex = {0};
				tex.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				tex.Transition.pResource = d3d12_data.texture_resource;
				tex.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				tex.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				tex.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

				D3D12_RESOURCE_BARRIER bars[] = { vb, tex };
				d3d12_data.cmd_list->ResourceBarrier(_countof(bars), bars);
			}



			d3d12_data.cmd_list->SetGraphicsRootSignature(d3d12_data.signature);
			d3d12_data.cmd_list->SetDescriptorHeaps(1, &d3d12_data.srv_heap);
			d3d12_data.cmd_list->SetGraphicsRootDescriptorTable(
				d3d12_data.table_slot,
				d3d12_data.srv_heap->GetGPUDescriptorHandleForHeapStart()
			);

			float consts[] = {
				(float)window_width,
				(float)window_height,
				window_aspect,
				(float)uptime,
			};
			d3d12_data.cmd_list->SetGraphicsRoot32BitConstants(d3d12_data.consts_slot, _countof(consts), consts, 0);

			D3D12_VIEWPORT viewport = {0};
			viewport.Width = (float)window_width;
			viewport.Height = (float)window_height;

			D3D12_RECT scissor = {0};
			scissor.right = (ULONG)window_width;
			scissor.bottom = (ULONG)window_height;

			d3d12_data.cmd_list->RSSetViewports(1, &viewport);
			d3d12_data.cmd_list->RSSetScissorRects(1, &scissor);

			UINT render_target_index = d3d12_data.swapchain->GetCurrentBackBufferIndex();

			D3D12_RESOURCE_BARRIER rt = {0};
			rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			rt.Transition.pResource = d3d12_data.render_targets[render_target_index];
			rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			rt.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			d3d12_data.cmd_list->ResourceBarrier(1, &rt);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d12_data.rtv_base;
			rtv_handle.ptr += d3d12_data.rtv_stride * render_target_index;
			d3d12_data.cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, NULL);

			d3d12_data.cmd_list->ClearRenderTargetView(rtv_handle, clear_color, 0, NULL);
			d3d12_data.cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d3d12_data.cmd_list->IASetVertexBuffers(0, 1, &d3d12_data.vbv);
			d3d12_data.cmd_list->DrawInstanced(_countof(vertex_data), 1, 0, 0);

			rt.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			rt.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

			d3d12_data.cmd_list->ResourceBarrier(1, &rt);

			hresult = d3d12_data.cmd_list->Close();
			ASSERT(SUCCEEDED(hresult));
		}

		// render
		{
			d3d12_data.cmd_queue->ExecuteCommandLists(1, (ID3D12CommandList**)&d3d12_data.cmd_list);

			hresult = d3d12_data.swapchain->Present(vsync_enabled ? 1 : 0, 0);
			ASSERT(SUCCEEDED(hresult));
		}

		// wait to finish
		{
			hresult = d3d12_data.cmd_queue->Signal(d3d12_data.fence, d3d12_data.fence_val);
			ASSERT(SUCCEEDED(hresult));

			if (d3d12_data.fence->GetCompletedValue() < d3d12_data.fence_val) {
				hresult = d3d12_data.fence->SetEventOnCompletion(d3d12_data.fence_val, d3d12_data.fence_event);
				ASSERT(SUCCEEDED(hresult));

				WaitForSingleObject(d3d12_data.fence_event, INFINITE);
			}

			d3d12_data.fence_val++;
		}
	}
	main_loop_end:

	// clean up
	d3d12_shutdown();
	DEBUGLOG("cleanup sucessful");

	return 0;
}




// initializes
static void d3d12_init(HWND window) {
	HRESULT hresult = {0};
	// initialize Direct3D 12
	{
		// create device
		{
			hresult = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_data.device));
			ASSERT(SUCCEEDED(hresult));

			D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {0};
			cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			hresult = d3d12_data.device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&d3d12_data.cmd_queue));
			ASSERT(SUCCEEDED(hresult));
		}
		DEBUGLOG("crated d3d12 device");

		// create swapchain
		{
			IDXGIFactory2* dxgi;

			hresult = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi));

			DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {0};
			swapchain_desc.Width = 0;
			swapchain_desc.Height = 0;
			swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchain_desc.SampleDesc = { 1, 0 };
			swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchain_desc.BufferCount = BUFFER_COUNT;
			swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

			hresult = dxgi->CreateSwapChainForHwnd(
				(IUnknown*)d3d12_data.cmd_queue,
				window,
				&swapchain_desc,
				NULL,
				NULL,
				(IDXGISwapChain1**)&d3d12_data.swapchain
			);
			ASSERT(SUCCEEDED(hresult));

			dxgi->Release();
		}
		DEBUGLOG("crated d3d12 d3d12_data.swapchain");

		// create root d3d12_data.signature
		{
			D3D12_DESCRIPTOR_RANGE range = {0};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.NumDescriptors = 1;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_ROOT_PARAMETER table = {0};
			table.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			table.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			table.DescriptorTable.NumDescriptorRanges = 1;
			table.DescriptorTable.pDescriptorRanges = &range;

			D3D12_ROOT_PARAMETER consts = {0};
			consts.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			consts.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			consts.Constants.Num32BitValues = 4;

			D3D12_ROOT_PARAMETER params[] = { table, consts };
			d3d12_data.table_slot = 0;
			d3d12_data.consts_slot = 1;

			D3D12_STATIC_SAMPLER_DESC sampler = {0};
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			D3D12_STATIC_SAMPLER_DESC samplers[] = { sampler };



			D3D12_ROOT_SIGNATURE_DESC signature_desc = {0};
			signature_desc.NumParameters = _countof(params);
			signature_desc.pParameters = params;
			signature_desc.NumStaticSamplers = _countof(samplers);
			signature_desc.pStaticSamplers = samplers;
			signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ID3DBlob* blob;
			hresult = D3D12SerializeRootSignature(
				&signature_desc,
				D3D_ROOT_SIGNATURE_VERSION_1,
				&blob,
				NULL
			);
			ASSERT(SUCCEEDED(hresult));

			hresult = d3d12_data.device->CreateRootSignature(
				0,
				blob->GetBufferPointer(),
				blob->GetBufferSize(),
				IID_PPV_ARGS(&d3d12_data.signature)
			);
			ASSERT(SUCCEEDED(hresult));

			blob->Release();
		}
		DEBUGLOG("crated d3d12 root d3d12_data.signature");

		// create pipeline state object
		{
			ID3DBlob* vs;
			ID3DBlob* ps;
			ID3DBlob* error;

			hresult = D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "vs", "vs_5_0", 0, 0, &vs, &error);
			if (FAILED(hresult)) {
				volatile const char* msg = (const char*)error->GetBufferPointer();
				OutputDebugStringA((const char*)msg);
				ASSERT(0 && "vertex shader error");
			}

			hresult = D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "ps", "ps_5_0", 0, 0, &ps, &error);
			if (FAILED(hresult)) {
				const char* msg = (const char*)error->GetBufferPointer();
				OutputDebugStringA(msg);
				ASSERT(0 && "pixel shader error");
			}

			// !!!!!!! DXGI_FORMAT_R32G32_FLOAT
			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(vertex_t, pos),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(vertex_t, uv),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, offsetof(vertex_t, col),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};

			D3D12_BLEND_DESC blend = {0};
			blend.RenderTarget[0].BlendEnable = TRUE;
			blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
			blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			D3D12_RASTERIZER_DESC rasterizer = {0};
			rasterizer.FillMode = D3D12_FILL_MODE_SOLID; // D3D12_FILL_MODE_SOLID or D3D12_FILL_MODE_WIREFRAME
			rasterizer.CullMode = D3D12_CULL_MODE_NONE;

			D3D12_DEPTH_STENCIL_DESC depth_stencil = {0};
			depth_stencil.DepthEnable = FALSE;
			depth_stencil.StencilEnable = FALSE;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {0};
			pipeline_desc.pRootSignature = d3d12_data.signature;
			pipeline_desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			pipeline_desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			pipeline_desc.BlendState = blend;
			pipeline_desc.SampleMask = UINT_MAX;
			pipeline_desc.RasterizerState = rasterizer;
			pipeline_desc.DepthStencilState = depth_stencil;
			pipeline_desc.InputLayout = { input_elems, _countof(input_elems) };
			pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipeline_desc.NumRenderTargets = 1;
			pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			pipeline_desc.SampleDesc = { 1, 0 };

			hresult = d3d12_data.device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&d3d12_data.pipeline));
			ASSERT(SUCCEEDED(hresult));

			vs->Release();
			ps->Release();
		}
		DEBUGLOG("crated d3d12 pipeline state object");

		// create command allocator and command list
		{
			hresult = d3d12_data.device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&d3d12_data.cmd_alloc)
			);
			ASSERT(SUCCEEDED(hresult));

			hresult = d3d12_data.device->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				d3d12_data.cmd_alloc,
				d3d12_data.pipeline,
				IID_PPV_ARGS(&d3d12_data.cmd_list)
			);
			ASSERT(SUCCEEDED(hresult));

			hresult = d3d12_data.cmd_list->Close();
			ASSERT(SUCCEEDED(hresult));
		}
		DEBUGLOG("crated d3d12 command allocator and command list");

		// create and upload buffer
		{
			D3D12_HEAP_PROPERTIES heap = {0};
			heap.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC buffer = {0};
			buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			buffer.Alignment = 0;
			buffer.Width = 64 * 1024;
			buffer.Height = 1;
			buffer.DepthOrArraySize = 1;
			buffer.MipLevels = 1;
			buffer.Format = DXGI_FORMAT_UNKNOWN;
			buffer.SampleDesc = { 1, 0 };
			buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			buffer.Flags = D3D12_RESOURCE_FLAG_NONE;

			hresult = d3d12_data.device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&buffer,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				NULL,
				IID_PPV_ARGS(&d3d12_data.upload_buffer)
			);
			ASSERT(SUCCEEDED(hresult));

			// upload the data
			uint8_t* p;
			hresult = d3d12_data.upload_buffer->Map(0, NULL, (void**)&p);
			ASSERT(SUCCEEDED(hresult));
			{
				// vertex data
				d3d12_data.vertex_offset = 0;
				memcpy(p, vertex_data, sizeof(vertex_data));

				// texture data
				d3d12_data.texture_offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
				p += d3d12_data.texture_offset;
				size_t stride = texture_size[0] * sizeof(texture_data[0]);
				for (size_t i = 0; i < sizeof(texture_data); i += stride) {
					memcpy(p, (uint8_t*)texture_data + i, stride);
					p += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
				}
			}
			d3d12_data.upload_buffer->Unmap(0, NULL);
		}
		DEBUGLOG("crated d3d12 upload buffer");

		// create vertex buffer
		{
			D3D12_HEAP_PROPERTIES heap = {0};
			heap.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC buffer = {0};
			buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			buffer.Alignment = 0;
			buffer.Width = sizeof(vertex_data);
			buffer.Height = 1;
			buffer.DepthOrArraySize = 1;
			buffer.MipLevels = 1;
			buffer.Format = DXGI_FORMAT_UNKNOWN;
			buffer.SampleDesc = { 1, 0 };
			buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			buffer.Flags = D3D12_RESOURCE_FLAG_NONE;

			hresult = d3d12_data.device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&buffer,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				IID_PPV_ARGS(&d3d12_data.vertex_resource)
			);
			ASSERT(SUCCEEDED(hresult));

			d3d12_data.vbv.BufferLocation = d3d12_data.vertex_resource->GetGPUVirtualAddress();
			d3d12_data.vbv.StrideInBytes = sizeof(vertex_t);
			d3d12_data.vbv.SizeInBytes = sizeof(vertex_data);
		}
		DEBUGLOG("crated d3d12 vertex resource");

		// create texture resource
		{
			D3D12_HEAP_PROPERTIES heap = {0};
			heap.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC texture = {0};
			texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texture.Alignment = 0;
			texture.Width = (UINT)texture_size[0];
			texture.Height = (UINT)texture_size[1];
			texture.DepthOrArraySize = 1;
			texture.MipLevels = 1;
			texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			texture.SampleDesc = { 1, 0 };
			texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texture.Flags = D3D12_RESOURCE_FLAG_NONE;

			hresult = d3d12_data.device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&texture,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				IID_PPV_ARGS(&d3d12_data.texture_resource)
			);
			ASSERT(SUCCEEDED(hresult));

			D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {0};
			srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srv_heap_desc.NumDescriptors = 1;
			srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			hresult = d3d12_data.device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&d3d12_data.srv_heap));
			ASSERT(SUCCEEDED(hresult));

			d3d12_data.device->CreateShaderResourceView(
				d3d12_data.texture_resource,
				NULL,
				d3d12_data.srv_heap->GetCPUDescriptorHandleForHeapStart()
			);
		}
		DEBUGLOG("crated d3d12 texture resource");

		// create a fence
		{
			hresult = d3d12_data.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12_data.fence));
			ASSERT(SUCCEEDED(hresult));

			d3d12_data.fence_val = 0;

			d3d12_data.fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
			ASSERT(d3d12_data.fence_event);
		}
		DEBUGLOG("crated d3d12 fence");
	}
}



// quit Direct3D 12
static void d3d12_shutdown() {
	for (int i = 0; i < BUFFER_COUNT; i++) {
		d3d12_data.render_targets[i]->Release();
	}
	d3d12_data.rtv_heap->Release();

	CloseHandle(d3d12_data.fence_event);
	d3d12_data.fence->Release();

	d3d12_data.srv_heap->Release();
	d3d12_data.texture_resource->Release();
	d3d12_data.vertex_resource->Release();
	d3d12_data.upload_buffer->Release();

	d3d12_data.cmd_list->Release();
	d3d12_data.cmd_alloc->Release();
	d3d12_data.pipeline->Release();
	d3d12_data.signature->Release();
	d3d12_data.swapchain->Release();
	d3d12_data.cmd_queue->Release();
	d3d12_data.device->Release();
}