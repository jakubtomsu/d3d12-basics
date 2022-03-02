<<<<<<< HEAD

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



static wchar_t const* window_title = L"d3d12-basics";
static int window_width = 720;
static int window_height = 480;
static float window_aspect = (float)window_width / (float)window_height;
static bool window_resized = false;




typedef struct vertex_t {
	float pos[2];
	float uv [2];
	float col[4];
} vertex_t;

static vertex_t vertex_data[] = {
	{{-0.0f,+0.7f}, {1.5f,0.0f}, {1.0f,0.0f,0.0f,1.0f}},
	{{+0.7f,-0.7f}, {3.0f,3.0f}, {0.0f,1.0f,0.0f,1.0f}},
	{{-0.7f,-0.7f}, {0.0f,3.0f}, {0.0f,0.0f,1.0f,1.0f}},
};

#define TEXTURE_DATA_WIDTH 2
#define TEXTURE_DATA_HEIGHT 2
static uint32_t texture_data[TEXTURE_DATA_WIDTH*TEXTURE_DATA_HEIGHT] = {
	0xaaaaaaaa, 0xffffffff,
	0xffffffff, 0xaaaaaaaa,
};

static float clear_color[] ={0.05f, 0.4f, 0.3f, 1.0f};
static bool vsync_enabled = true;

FILE* debug_log_file;

#define DEBUGLOG(str) fprintf(debug_log_file, str "\n")



#define ASSERT(expr) assert(expr)


static LRESULT CALLBACK
window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
	LRESULT lr = 0;

	switch(message) {
		case WM_SIZE: {
			window_width = LOWORD(lp);
			window_height = HIWORD(lp);
			window_aspect = (float)window_width / (float)window_height;
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
int WINAPI
WinMain(HINSTANCE instance, HINSTANCE instance_p, LPSTR cmd_line, int cmd_show) {
	debug_log_file = fopen("log.txt", "w");

	// create window
	HWND window;
	{
		WNDCLASSEXW wc = {0};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.lpfnWndProc = window_proc;
		wc.hInstance = instance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.lpszClassName = window_title;

		if(!RegisterClassExW(&wc)) {
			ASSERT(0);
		}

		DWORD style = WS_OVERLAPPEDWINDOW;
		DWORD style_ex = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;

		window = CreateWindowExW(
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
	
		if(!window) {
			ASSERT(0);
		}

		ShowWindow(window, cmd_show);
	}
	DEBUGLOG("crated window");

	// create device
	ID3D12Device* device;
	ID3D12CommandQueue* cmd_queue;
	{
		HRESULT hresult;

		hresult = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
		ASSERT(SUCCEEDED(hresult));

		D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {0};
		cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		hresult = device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue));
		ASSERT(SUCCEEDED(hresult));
	}
	DEBUGLOG("crated d3d12 device");

	// create swapchain
	IDXGISwapChain3* swapchain;
	UINT buffer_count = 2;
	{
		IDXGIFactory2* dxgi;
		HRESULT hresult;

		hresult = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi));

		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {0};
		swapchain_desc.Width		= 0;
		swapchain_desc.Height		= 0;
		swapchain_desc.Format		= DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc.SampleDesc	= {1, 0};
		swapchain_desc.BufferUsage	= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_desc.BufferCount	= buffer_count;
		swapchain_desc.SwapEffect	= DXGI_SWAP_EFFECT_FLIP_DISCARD;

		hresult = dxgi->CreateSwapChainForHwnd(
			(IUnknown*)cmd_queue,
			window,
			&swapchain_desc,
			NULL,
			NULL,
			(IDXGISwapChain1**)&swapchain
		);
		ASSERT(SUCCEEDED(hresult));

		dxgi->Release();
	}
	DEBUGLOG("crated d3d12 swapchain");

	// create root signature
	ID3D12RootSignature* signature;
	UINT table_slot;
	UINT consts_slot;
	{
		HRESULT hresult;

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
		consts.Constants.Num32BitValues		= 4;

		D3D12_ROOT_PARAMETER params[] = {table, consts};
		table_slot = 0;
		consts_slot = 1;

		D3D12_STATIC_SAMPLER_DESC sampler = {0};
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

		D3D12_STATIC_SAMPLER_DESC samplers[] = {sampler};



		D3D12_ROOT_SIGNATURE_DESC signature_desc = {0};
		signature_desc.NumParameters		= _countof(params);
		signature_desc.pParameters		= params;
		signature_desc.NumStaticSamplers	= _countof(samplers);
		signature_desc.pStaticSamplers		= samplers;
		signature_desc.Flags			= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* blob;
		hresult = D3D12SerializeRootSignature(
			&signature_desc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&blob,
			NULL
		);
		ASSERT(SUCCEEDED(hresult));

		hresult = device->CreateRootSignature(
			0,
			blob->GetBufferPointer(),
			blob->GetBufferSize(),
			IID_PPV_ARGS(&signature)
		);
		ASSERT(SUCCEEDED(hresult));

		blob->Release();
	}
	DEBUGLOG("crated d3d12 root signature");

	// create pipeline state object
	ID3D12PipelineState* pipeline;
	{
		HRESULT hresult;
		ID3DBlob* vs;
		ID3DBlob* ps;
		ID3DBlob* error;

		hresult = D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "vs", "vs_5_0", 0, 0, &vs, &error);
		if(FAILED(hresult)) {
			const char* msg = (const char*)error->GetBufferPointer();
			OutputDebugStringA(msg);
			ASSERT(0 && "vs error");
		}

		hresult = D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "ps", "ps_5_0", 0, 0, &ps, &error);
		if(FAILED(hresult)) {
			const char* msg = (const char*)error->GetBufferPointer();
			OutputDebugStringA(msg);
			ASSERT(0 && "ps error");
		}

		// !!!!!!! DXGI_FORMAT_R32G32_FLOAT
		D3D12_INPUT_ELEMENT_DESC input_elems[] = {
			{"POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(vertex_t, pos),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(vertex_t, uv),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, offsetof(vertex_t, col),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

		D3D12_BLEND_DESC blend = {0};
		blend.RenderTarget[0].BlendEnable = TRUE;
		blend.RenderTarget[0].SrcBlend  = D3D12_BLEND_SRC_ALPHA;
		blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_RASTERIZER_DESC rasterizer = {0};
		rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizer.CullMode = D3D12_CULL_MODE_NONE;

		D3D12_DEPTH_STENCIL_DESC depth_stencil = {0};
		depth_stencil.DepthEnable = FALSE;
		depth_stencil.StencilEnable = FALSE;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {0};
		pipeline_desc.pRootSignature = signature;
		pipeline_desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
		pipeline_desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
		pipeline_desc.BlendState = blend;
		pipeline_desc.SampleMask = UINT_MAX;
		pipeline_desc.RasterizerState = rasterizer;
		pipeline_desc.DepthStencilState = depth_stencil;
		pipeline_desc.InputLayout = {input_elems, _countof(input_elems)};
		pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipeline_desc.NumRenderTargets = 1;
		pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipeline_desc.SampleDesc = {1, 0};

		hresult = device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline));
		ASSERT(SUCCEEDED(hresult));

		vs->Release();
		ps->Release();
	}
	DEBUGLOG("crated d3d12 pipeline state object");

	// create command allocator and command list
	ID3D12CommandAllocator* cmd_alloc;
	ID3D12GraphicsCommandList* cmd_list;
	{
		HRESULT hresult;

		hresult = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&cmd_alloc)
		);
		ASSERT(SUCCEEDED(hresult));

		hresult = device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			cmd_alloc,
			pipeline,
			IID_PPV_ARGS(&cmd_list)
		);
		ASSERT(SUCCEEDED(hresult));

		hresult = cmd_list->Close();
		ASSERT(SUCCEEDED(hresult));
	}
	DEBUGLOG("crated d3d12 command allocator and command list");

	// create and upload buffer
	ID3D12Resource* upload_buffer;
	SIZE_T vertex_offset;
	SIZE_T texture_offset;
	{
		HRESULT hresult;

		D3D12_HEAP_PROPERTIES heap = {0};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC buffer = {0};
		buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffer.Alignment = 0;
		buffer.Width = 64*1024;
		buffer.Height = 1;
		buffer.DepthOrArraySize = 1;
		buffer.MipLevels = 1;
		buffer.Format = DXGI_FORMAT_UNKNOWN;
		buffer.SampleDesc = {1, 0};
		buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffer.Flags = D3D12_RESOURCE_FLAG_NONE;

		hresult = device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL,
			IID_PPV_ARGS(&upload_buffer)
		);
		ASSERT(SUCCEEDED(hresult));

		// upload the data
		uint8_t* p;
		hresult = upload_buffer->Map(0, NULL, (void**)&p);
		ASSERT(SUCCEEDED(hresult));
		{
			// vertex data
			vertex_offset = 0;
			memcpy(p, vertex_data, sizeof(vertex_data));

			// texture data
			texture_offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
			p += texture_offset;
			size_t stride = TEXTURE_DATA_WIDTH * sizeof(texture_data[0]);
			for(size_t i = 0; i < sizeof(texture_data); i += stride) {
				memcpy(p, (uint8_t*)texture_data + i, stride);
				p += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
			}
		}
		upload_buffer->Unmap(0, NULL);
	}
	DEBUGLOG("crated d3d12 upload buffer");

	// create vertex buffer
	ID3D12Resource* vertex_resource;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	{
		HRESULT hresult;

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
		buffer.SampleDesc = {1, 0};
		buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffer.Flags = D3D12_RESOURCE_FLAG_NONE;

		hresult = device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_COPY_DEST,
			NULL,
			IID_PPV_ARGS(&vertex_resource)
		);
		ASSERT(SUCCEEDED(hresult));

		vbv.BufferLocation = vertex_resource->GetGPUVirtualAddress();
		vbv.StrideInBytes = sizeof(vertex_t);
		vbv.SizeInBytes = sizeof(vertex_data);
	}
	DEBUGLOG("crated d3d12 vertex resource");

	// create texture resource
	ID3D12Resource* texture_resource;
	ID3D12DescriptorHeap* srv_heap;
	{
		HRESULT hresult;

		D3D12_HEAP_PROPERTIES heap = {0};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC texture = {0};
		texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texture.Alignment = 0;
		texture.Width = (UINT)TEXTURE_DATA_WIDTH;
		texture.Height = (UINT)TEXTURE_DATA_HEIGHT;
		texture.DepthOrArraySize = 1;
		texture.MipLevels = 1;
		texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture.SampleDesc = {1, 0};
		texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texture.Flags = D3D12_RESOURCE_FLAG_NONE;

		hresult = device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&texture,
			D3D12_RESOURCE_STATE_COPY_DEST,
			NULL,
			IID_PPV_ARGS(&texture_resource)
		);
		ASSERT(SUCCEEDED(hresult));

		D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {0};
		srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srv_heap_desc.NumDescriptors = 1;
		srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hresult = device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&srv_heap));
		ASSERT(SUCCEEDED(hresult));

		device->CreateShaderResourceView(
			texture_resource,
			NULL,
			srv_heap->GetCPUDescriptorHandleForHeapStart()
		);
	}
	DEBUGLOG("crated d3d12 texture resource");

	// create a fence
	ID3D12Fence* fence;
	UINT64 fence_val;
	HANDLE fence_event;
	{
		HRESULT hresult;

		hresult = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		ASSERT(SUCCEEDED(hresult));

		fence_val = 0;

		fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		ASSERT(fence_event);
	}
	DEBUGLOG("crated d3d12 fence");




	// program loop

	ID3D12Resource* render_targets[2]; // buffer_count
	ID3D12DescriptorHeap* rtv_heap = NULL;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_base;
	UINT rtv_stride;

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

	while(1) {
		MSG msg;
		if(PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if(msg.message == WM_QUIT) {
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		DEBUGLOG("frame\n");

		// create the render targets
		if(window_resized) {
			window_resized = false;

			if(rtv_heap) {
				cmd_list->ClearState(NULL);
				for(UINT i = 0; i < buffer_count; i++) {
					render_targets[i]->Release();
				}
				rtv_heap->Release();
			}

			HRESULT hresult;

			hresult = swapchain->ResizeBuffers(
				buffer_count,
				window_width,
				window_height,
				DXGI_FORMAT_UNKNOWN,
				0
			);
			ASSERT(SUCCEEDED(hresult));

			D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {0};
			rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtv_heap_desc.NumDescriptors = buffer_count;

			hresult = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap));
			ASSERT(SUCCEEDED(hresult));

			rtv_base = rtv_heap->GetCPUDescriptorHandleForHeapStart();
			rtv_stride = device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV
			);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_base;
			for(UINT i = 0; i < buffer_count; i++) {
				hresult = swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
				ASSERT(SUCCEEDED(hresult));

				device->CreateRenderTargetView(render_targets[i], NULL, rtv_handle);
				rtv_handle.ptr += rtv_stride;
			}
		}

		// fill command lists
		{
			HRESULT hresult;

			hresult = cmd_alloc->Reset();
			ASSERT(SUCCEEDED(hresult));

			hresult = cmd_list->Reset(cmd_alloc, pipeline);
			ASSERT(SUCCEEDED(hresult));

			if(first_time) {
				first_time = false;

				cmd_list->CopyBufferRegion(
					vertex_resource,
					0,
					upload_buffer,
					vertex_offset,
					sizeof(vertex_data)
				);

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {0};

				device->GetCopyableFootprints(
					&texture_resource->GetDesc(),
					0,
					1,
					texture_offset,
					&footprint,
					NULL,
					NULL,
					NULL
				);

				D3D12_TEXTURE_COPY_LOCATION src = {0};
				src.pResource = upload_buffer;
				src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				src.PlacedFootprint = footprint;

				D3D12_TEXTURE_COPY_LOCATION dst = {0};
				dst.pResource = texture_resource;
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = 0;

				cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

				

				D3D12_RESOURCE_BARRIER vb = {0};
				vb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				vb.Transition.pResource = vertex_resource;
				vb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				vb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				vb.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

				D3D12_RESOURCE_BARRIER tex = {0};
				tex.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				tex.Transition.pResource = texture_resource;
				tex.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				tex.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				tex.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

				D3D12_RESOURCE_BARRIER bars[] = {vb, tex};
				cmd_list->ResourceBarrier(_countof(bars), bars);
			}



			cmd_list->SetGraphicsRootSignature(signature);
			cmd_list->SetDescriptorHeaps(1, &srv_heap);
			cmd_list->SetGraphicsRootDescriptorTable(
				table_slot,
				srv_heap->GetGPUDescriptorHandleForHeapStart()
			);

			float consts[] = {
				(float)window_width,
				(float)window_height,
				window_aspect,
				(float)uptime,
			};
			cmd_list->SetGraphicsRoot32BitConstants(consts_slot, _countof(consts), consts, 0);

			D3D12_VIEWPORT viewport = {0};
			viewport.Width = (float)window_width;
			viewport.Height = (float)window_height;

			D3D12_RECT scissor = {0};
			scissor.right = (ULONG)window_width;
			scissor.bottom = (ULONG)window_height;

			cmd_list->RSSetViewports(1, &viewport);
			cmd_list->RSSetScissorRects(1, &scissor);

			UINT render_target_index = swapchain->GetCurrentBackBufferIndex();

			D3D12_RESOURCE_BARRIER rt = {0};
			rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			rt.Transition.pResource = render_targets[render_target_index];
			rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			rt.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			cmd_list->ResourceBarrier(1, &rt);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_base;
			rtv_handle.ptr += rtv_stride * render_target_index;
			cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, NULL);

			cmd_list->ClearRenderTargetView(rtv_handle, clear_color, 0, NULL);
			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd_list->IASetVertexBuffers(0, 1, &vbv);
			cmd_list->DrawInstanced(_countof(vertex_data), 1, 0, 0);

			rt.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			rt.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

			cmd_list->ResourceBarrier(1, &rt);

			hresult = cmd_list->Close();
			ASSERT(SUCCEEDED(hresult));
		}

		// render
		{
			HRESULT hresult;

			cmd_queue->ExecuteCommandLists(1, (ID3D12CommandList**)&cmd_list);

			hresult = swapchain->Present(vsync_enabled ? 1 : 0, 0);
			ASSERT(SUCCEEDED(hresult));
		}

		// wait to finish
		{
			HRESULT hresult;

			hresult = cmd_queue->Signal(fence, fence_val);
			ASSERT(SUCCEEDED(hresult));

			if(fence->GetCompletedValue() < fence_val) {
				hresult = fence->SetEventOnCompletion(fence_val, fence_event);
				ASSERT(SUCCEEDED(hresult));

				WaitForSingleObject(fence_event, INFINITE);
			}
			
			fence_val++;
		}
	}

	return 0;
}
=======

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

FILE* debug_log_file;



// d3d12 data
HWND window;
ID3D12Device* device;
ID3D12CommandQueue* cmd_queue;
IDXGISwapChain3* swapchain;
UINT buffer_count = 2;
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

ID3D12Resource* render_targets[2]; // buffer_count
ID3D12DescriptorHeap* rtv_heap = NULL;
D3D12_CPU_DESCRIPTOR_HANDLE rtv_base;
UINT rtv_stride;

bool should_run;






typedef struct vertex_t {
	float pos[2];
	float uv[2];
	float col[4];
} vertex_t;

static vertex_t vertex_data[] = {
	{{-0.0f,  0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
	{{ 0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
	{{-0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
};

static uint32_t texture_data[] = {
	0xaa000000, 0x88ffffff,
	0x88ffffff, 0xaa000000,
};
static size_t texture_size[2] = { 2, 2 };

static float clear_color[] = { 0.117f, 0.3f, 0.4f, 1.0f };
static bool vsync_enabled = true;






static LRESULT CALLBACK
window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
	LRESULT lr = 0;

	switch (message) {
	case WM_SIZE: {
		window_width = LOWORD(lp);
		window_height = HIWORD(lp);
		window_aspect = (float)window_width / (float)window_height;
		DEBUGLOG("window resized to %i %i", window_width, window_height);
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
int WINAPI
WinMain(HINSTANCE instance, HINSTANCE instance_p, LPSTR cmd_line, int cmd_show) {
	debug_log_file = fopen("log.txt", "w");

	HRESULT hresult; // we can just reuse this

	// create window
	{
		WNDCLASSEXW wc = {};
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

		window = CreateWindowExW(
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

		if (!window) {
			ASSERT(0);
		}

		ShowWindow(window, cmd_show); // TODO: do this after d3d12 init
	}
	DEBUGLOG("crated window");

	// D3D12 init
	{
		// create device
		{
			HRESULT hresult;

			hresult = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
			ASSERT(SUCCEEDED(hresult));

			D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
			cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			hresult = device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue));
			ASSERT(SUCCEEDED(hresult));
		}
		DEBUGLOG("crated d3d12 device");

		// create swapchain
		{
			IDXGIFactory2* dxgi;

			hresult = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi));

			DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
			swapchain_desc.Width = 0;
			swapchain_desc.Height = 0;
			swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchain_desc.SampleDesc = { 1, 0 };
			swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchain_desc.BufferCount = buffer_count;
			swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

			hresult = dxgi->CreateSwapChainForHwnd(
				(IUnknown*)cmd_queue,
				window,
				&swapchain_desc,
				NULL,
				NULL,
				(IDXGISwapChain1**)&swapchain
			);
			ASSERT(SUCCEEDED(hresult));

			dxgi->Release();
		}
		DEBUGLOG("crated d3d12 swapchain");

		// create root signature
		{
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.NumDescriptors = 1;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			D3D12_ROOT_PARAMETER table = {};
			table.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			table.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			table.DescriptorTable.NumDescriptorRanges = 1;
			table.DescriptorTable.pDescriptorRanges = &range;

			D3D12_ROOT_PARAMETER consts = {};
			consts.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			consts.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			consts.Constants.Num32BitValues = 4;

			D3D12_ROOT_PARAMETER params[] = { table, consts };
			table_slot = 0;
			consts_slot = 1;

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			D3D12_STATIC_SAMPLER_DESC samplers[] = { sampler };



			D3D12_ROOT_SIGNATURE_DESC signature_desc = {};
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

			hresult = device->CreateRootSignature(
				0,
				blob->GetBufferPointer(),
				blob->GetBufferSize(),
				IID_PPV_ARGS(&signature)
			);
			ASSERT(SUCCEEDED(hresult));

			blob->Release();
		}
		DEBUGLOG("crated d3d12 root signature");

		// create pipeline state object
		{
			ID3DBlob* vs;
			ID3DBlob* ps;
			ID3DBlob* error;

			hresult = D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "vs", "vs_5_0", 0, 0, &vs, &error);
			if (FAILED(hresult)) {
				const char* msg = (const char*)error->GetBufferPointer();
				OutputDebugStringA(msg);
				ASSERT(0 && "vs error");
			}

			hresult = D3DCompileFromFile(L"shaders.hlsl", NULL, NULL, "ps", "ps_5_0", 0, 0, &ps, &error);
			if (FAILED(hresult)) {
				const char* msg = (const char*)error->GetBufferPointer();
				OutputDebugStringA(msg);
				ASSERT(0 && "ps error");
			}

			// !!!!!!! DXGI_FORMAT_R32G32_FLOAT
			D3D12_INPUT_ELEMENT_DESC input_elems[] = {
				{"POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(vertex_t, pos),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(vertex_t, uv),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
				{"COLOR",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, offsetof(vertex_t, col),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};

			D3D12_BLEND_DESC blend = {};
			blend.RenderTarget[0].BlendEnable = TRUE;
			blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
			blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			D3D12_RASTERIZER_DESC rasterizer = {};
			rasterizer.FillMode = D3D12_FILL_MODE_SOLID; // D3D12_FILL_MODE_SOLID or D3D12_FILL_MODE_WIREFRAME
			rasterizer.CullMode = D3D12_CULL_MODE_NONE;

			D3D12_DEPTH_STENCIL_DESC depth_stencil = {};
			depth_stencil.DepthEnable = FALSE;
			depth_stencil.StencilEnable = FALSE;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {};
			pipeline_desc.pRootSignature = signature;
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

			hresult = device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline));
			ASSERT(SUCCEEDED(hresult));

			vs->Release();
			ps->Release();
		}
		DEBUGLOG("crated d3d12 pipeline state object");

		// create command allocator and command list
		{
			hresult = device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&cmd_alloc)
			);
			ASSERT(SUCCEEDED(hresult));

			hresult = device->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				cmd_alloc,
				pipeline,
				IID_PPV_ARGS(&cmd_list)
			);
			ASSERT(SUCCEEDED(hresult));

			hresult = cmd_list->Close();
			ASSERT(SUCCEEDED(hresult));
		}
		DEBUGLOG("crated d3d12 command allocator and command list");

		// create and upload buffer
		{
			D3D12_HEAP_PROPERTIES heap = {};
			heap.Type = D3D12_HEAP_TYPE_UPLOAD;

			D3D12_RESOURCE_DESC buffer = {};
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

			hresult = device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&buffer,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				NULL,
				IID_PPV_ARGS(&upload_buffer)
			);
			ASSERT(SUCCEEDED(hresult));

			// upload the data
			uint8_t* p;
			hresult = upload_buffer->Map(0, NULL, (void**)&p);
			ASSERT(SUCCEEDED(hresult));
			{
				// vertex data
				vertex_offset = 0;
				memcpy(p, vertex_data, sizeof(vertex_data));

				// texture data
				texture_offset = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
				p += texture_offset;
				size_t stride = texture_size[0] * sizeof(texture_data[0]);
				for (size_t i = 0; i < sizeof(texture_data); i += stride) {
					memcpy(p, (uint8_t*)texture_data + i, stride);
					p += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
				}
			}
			upload_buffer->Unmap(0, NULL);
		}
		DEBUGLOG("crated d3d12 upload buffer");

		// create vertex buffer
		{
			D3D12_HEAP_PROPERTIES heap = {};
			heap.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC buffer = {};
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

			hresult = device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&buffer,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				IID_PPV_ARGS(&vertex_resource)
			);
			ASSERT(SUCCEEDED(hresult));

			vbv.BufferLocation = vertex_resource->GetGPUVirtualAddress();
			vbv.StrideInBytes = sizeof(vertex_t);
			vbv.SizeInBytes = sizeof(vertex_data);
		}
		DEBUGLOG("crated d3d12 vertex resource");

		// create texture resource
		{
			D3D12_HEAP_PROPERTIES heap = {};
			heap.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC texture = {};
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

			hresult = device->CreateCommittedResource(
				&heap,
				D3D12_HEAP_FLAG_NONE,
				&texture,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				IID_PPV_ARGS(&texture_resource)
			);
			ASSERT(SUCCEEDED(hresult));

			D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {};
			srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srv_heap_desc.NumDescriptors = 1;
			srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			hresult = device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&srv_heap));
			ASSERT(SUCCEEDED(hresult));

			device->CreateShaderResourceView(
				texture_resource,
				NULL,
				srv_heap->GetCPUDescriptorHandleForHeapStart()
			);
		}
		DEBUGLOG("crated d3d12 texture resource");

		// create a fence
		{
			hresult = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
			ASSERT(SUCCEEDED(hresult));

			fence_val = 0;

			fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
			ASSERT(fence_event);
		}
		DEBUGLOG("crated d3d12 fence");
	}

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

	should_run = true;

	while (should_run) {
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

		//DEBUGLOG("frame\n");

		// create the render targets
		if (window_resized) {
			window_resized = false;

			if (rtv_heap) {
				cmd_list->ClearState(NULL);
				for (UINT i = 0; i < buffer_count; i++) {
					render_targets[i]->Release();
				}
				rtv_heap->Release();
			}

			HRESULT hresult;

			hresult = swapchain->ResizeBuffers(
				buffer_count,
				window_width,
				window_height,
				DXGI_FORMAT_UNKNOWN,
				0
			);
			ASSERT(SUCCEEDED(hresult));

			D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
			rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtv_heap_desc.NumDescriptors = buffer_count;

			hresult = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap));
			ASSERT(SUCCEEDED(hresult));

			rtv_base = rtv_heap->GetCPUDescriptorHandleForHeapStart();
			rtv_stride = device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV
			);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_base;
			for (UINT i = 0; i < buffer_count; i++) {
				hresult = swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
				ASSERT(SUCCEEDED(hresult));

				device->CreateRenderTargetView(render_targets[i], NULL, rtv_handle);
				rtv_handle.ptr += rtv_stride;
			}
		}

		// fill command lists
		{
			HRESULT hresult;

			hresult = cmd_alloc->Reset();
			ASSERT(SUCCEEDED(hresult));

			hresult = cmd_list->Reset(cmd_alloc, pipeline);
			ASSERT(SUCCEEDED(hresult));

			if (first_time) {
				first_time = false;

				cmd_list->CopyBufferRegion(
					vertex_resource,
					0,
					upload_buffer,
					vertex_offset,
					sizeof(vertex_data)
				);

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};

				device->GetCopyableFootprints(
					&texture_resource->GetDesc(),
					0,
					1,
					texture_offset,
					&footprint,
					NULL,
					NULL,
					NULL
				);

				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = upload_buffer;
				src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				src.PlacedFootprint = footprint;

				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = texture_resource;
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = 0;

				cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);



				D3D12_RESOURCE_BARRIER vb = {};
				vb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				vb.Transition.pResource = vertex_resource;
				vb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				vb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				vb.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

				D3D12_RESOURCE_BARRIER tex = {};
				tex.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				tex.Transition.pResource = texture_resource;
				tex.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				tex.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				tex.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

				D3D12_RESOURCE_BARRIER bars[] = { vb, tex };
				cmd_list->ResourceBarrier(_countof(bars), bars);
			}



			cmd_list->SetGraphicsRootSignature(signature);
			cmd_list->SetDescriptorHeaps(1, &srv_heap);
			cmd_list->SetGraphicsRootDescriptorTable(
				table_slot,
				srv_heap->GetGPUDescriptorHandleForHeapStart()
			);

			float consts[] = {
				(float)window_width,
				(float)window_height,
				window_aspect,
				(float)uptime,
			};
			cmd_list->SetGraphicsRoot32BitConstants(consts_slot, _countof(consts), consts, 0);

			D3D12_VIEWPORT viewport = {};
			viewport.Width = (float)window_width;
			viewport.Height = (float)window_height;

			D3D12_RECT scissor = {};
			scissor.right = (ULONG)window_width;
			scissor.bottom = (ULONG)window_height;

			cmd_list->RSSetViewports(1, &viewport);
			cmd_list->RSSetScissorRects(1, &scissor);

			UINT render_target_index = swapchain->GetCurrentBackBufferIndex();

			D3D12_RESOURCE_BARRIER rt = {};
			rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			rt.Transition.pResource = render_targets[render_target_index];
			rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			rt.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			cmd_list->ResourceBarrier(1, &rt);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_base;
			rtv_handle.ptr += rtv_stride * render_target_index;
			cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, NULL);

			cmd_list->ClearRenderTargetView(rtv_handle, clear_color, 0, NULL);
			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmd_list->IASetVertexBuffers(0, 1, &vbv);
			cmd_list->DrawInstanced(_countof(vertex_data), 1, 0, 0);

			rt.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			rt.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

			cmd_list->ResourceBarrier(1, &rt);

			hresult = cmd_list->Close();
			ASSERT(SUCCEEDED(hresult));
		}

		// render
		{
			HRESULT hresult;

			cmd_queue->ExecuteCommandLists(1, (ID3D12CommandList**)&cmd_list);

			hresult = swapchain->Present(vsync_enabled ? 1 : 0, 0);
			ASSERT(SUCCEEDED(hresult));
		}

		// wait to finish
		{
			HRESULT hresult;

			hresult = cmd_queue->Signal(fence, fence_val);
			ASSERT(SUCCEEDED(hresult));

			if (fence->GetCompletedValue() < fence_val) {
				hresult = fence->SetEventOnCompletion(fence_val, fence_event);
				ASSERT(SUCCEEDED(hresult));

				WaitForSingleObject(fence_event, INFINITE);
			}

			fence_val++;
		}
	}
	main_loop_end:

	return 0;
}

>>>>>>> 61f57d7390201c82955e27e90b5dd96c33786604
