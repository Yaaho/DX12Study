#include "Stdafx.h"
#include "D3D12HelloTexture.h"


// static_cast: 컴파일 타임에 형변환에 대한 타입 오류를 잡아준다.
D3D12HelloTexture::D3D12HelloTexture(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0)
{
}


void D3D12HelloTexture::OnInit()
{
    LoadPipeline();
    LoadAssets();
}


// Load the rendering pipeline dependencies.
void D3D12HelloTexture::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    // 디버그 레이어를 활성화한다.
    // 장치 생성 후 디버그 레이어를 활성화하면 활성 장치가 무효화된다.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            // 추가 디버그 레이어를 사용한다.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

    // Describe and create the command queue.
    // command queue desc 를 작성한다.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    // swap chain 을 desc 하고 생성한다.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    // This sample does not support fullscreen transitions.
    // alt + enter 를 눌렀을 때 전체화면으로 전환되지 않는다.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();


    // Create descriptor heaps.
    // 렌더 타겟과 셰이더 리소스의 Descriptor heaps 을 작성한다
    // DX12 에서 descriptor(서술자) 는 DX11 의 View 와 동의어이다.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        // Describe and create a shader resource view (SRV) heap for the texture.
        // 셰이더 리소스 뷰를 위한 DESCRIPTOR HEAP 을 생성한다.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 1;
        // DESCRIPTOR Type 을 정한다.
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
        
        // RTV Descriptor 의 증가값을 저장해둔다.
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }



    // Create frame resources.
     // 프레임 리소스를 만든다.
    {
        // GetCPUDescriptorHandleForHeapStart 함수로 힙의 첫 디스크립터 핸들을 가져온 후
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        // 각 프레임에 대해 RTV 를 만든다.
        for (UINT n = 0; n < FrameCount; n++)
        {
            // 렌더 타겟을 생성하고
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            // m_rtvDescriptorSize 만큼 이동한다.
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    // CommandAllocator 를 생성한다.
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}


// Load the sample assets.
void D3D12HelloTexture::LoadAssets()
{
    // Create the root signature.
    // root signature 는 그리기 호출 전에 렌더링 파이프라인에 묶이는 자원들을 정의하고, 
    // 그 자원들이 셰이더의 입력 레지스터에 어떻게 대응되는지를 정의한다.
    {
        // ROOT_SIGNATURE 버전 지원 확인 용으로 사용하는 구조체
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        // 그래픽카드가 루트 시그니처 1_1 을 지원하지 않으면 1_0 으로 내림
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // descriptor table 루트 파라미터를 정의하려면 
        // CD3DX12_ROOT_PARAMETER 의 멤버를 채워서 descriptor table 에 대한 좀 더 구체적인 정보를 제공해야 한다.
        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        // CD3DX12_DESCRIPTOR_RANGE 배열과 배열의 구간(range) 의 개수를 지정하고 Descriptor Table 을 Init 해준다.
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

        // 샘플러 정의
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        // 포인트 샘플링 적용
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        // 수평
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        // 수직
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        // 깊이
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        
        // 루트 시그니처를 생성하기 전 만들어둔 것들을 직렬화
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        // 루트 시그니처 생성
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        // 디버그 파일 / 행 / 형식 / 기호 정보를 출력 코드에 삽입하도록 컴파일러에 지시한다.
        // 코드 생성 중에 최적화 단계를 건너뛰도록 컴파일러에 지시한다.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif
        ThrowIfFailed(D3DCompileFromFile(L"Shaders.HLSL", nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"Shaders.HLSL", nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        // Vertex 구조체의 각 성분을 서술한다.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            // UV 좌표이므로 RG 만 사용
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        // graphics pipeline state object 를 서술한다.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        // 위에서 버텍스 Desc
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        // 위에서 생성한 루트 시그니쳐
        psoDesc.pRootSignature = m_rootSignature.Get();
        // 위에서 불러온 셰이더
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        // blend state 의 샘플 마스크
        psoDesc.SampleMask = UINT_MAX;
        // 기본 도형의 위상 구조 종류를 지정, patch 속성은 테셀레이션에서 쓰인다고 한다.
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        // 동시에 사용할 렌더 타겟 갯수
        psoDesc.NumRenderTargets = 1;
        // 렌더 타겟의 포맷
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }


    // Create the command list.
    // command list 생성
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));


    // Create the vertex buffer.
    // 버텍스 버퍼 생성
    // 이 데모에서는 UploadBuffer만을 만들어 주었지만,
    // DefaultBuffer를 같이 만들어주어서, UpdateSubResources를 통해 업로드버퍼에서 일반버퍼로 복사해주는 방식을 추천한다.
    // 이 방법이 장기적으로 봤을때는 성능관리 측면에서 좋다고 한다.
    {
        // Define the geometry for a triangle.
        // 정점의 위치, 컬러값
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        // 버텍스 버퍼와 같은 정점 데이터를 전송하기 위해 업로드 힙을 사용하는 것은 권장되지 않는다.
        // GPU 에서 필요로 할 때마다 업로드 힙이 정렬된다. 기본 힙 사용법을 읽어보라.
        // 여기서 업로드 힙은 코드 단순화를 목적으로, 그리고 실제로 전송할 수 있는 버트(verts) 가 거의 없기 때문에 사용되는 것이다
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        // 삼각형의 데이터를 정점 버퍼에 복사한다.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);      // We do not intend to read from this resource on the CPU.
        // 자료를 복사하기 전 map 호출
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        // 자료를 모두 복사한 후에 unmap 호출
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        // 정점 버퍼 뷰를 초기화
        // 정점 버퍼 뷰는 descriptor 를 필요로 하지 않는다.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }


    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    // ComPtr 은 CPU 개체이지만 이 리소스는 이를 참조하는 커맨드 리스트가 GPU 에서 실행을 마칠때까지 스코프에 있어야 한다.
    // 이 자원이 조기에 파괴되지 않도록 이 방법의 끝에서 GPU 를 플러시한다.
    // 텍스쳐를 업로드하는 버퍼로 쓰인다
    ComPtr<ID3D12Resource> textureUploadHeap;

    // Create the texture.
    {
        // Describe and create a Texture2D.
        // 텍스쳐에 관한 정보를 서술한다.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = TextureWidth;
        textureDesc.Height = TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        // 자원을 생성하고 지정된 속성들에 부합하는 힙에 그 자원을 맡긴다(commit)
        ThrowIfFailed(m_device->CreateCommittedResource(
            // D3D12_HEAP_TYPE_DEFAULT 는 기본 힙이며 전적으로 GPU 가 접근할 자원들이 담긴다.
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            // 자원을 맡길 힙이 가졌으면 하는 속성을 나타내는 플래그
            D3D12_HEAP_FLAG_NONE,
            // 텍스쳐의 Desc
            &textureDesc,
            // 자원의 초기 상태를 지정
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_texture)));

        // 업로드할 버퍼의 사이즈를 저장
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

        // Create the GPU upload buffer
        // GPU 업로드 버퍼를 생성
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&textureUploadHeap)));

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.
        // 텍스쳐를 생성하고
        // 중간 업로드 힙에 데이터를 복사한다음
        // 업로드 힙에서 Texture2D 로 복사본을 예약한다.
        std::vector<UINT8> texture = GenerateTextureData();

        // TextureData 를 서술한다.
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        // 위에서 작성한 서브리소스를 업데이트 한다.
        UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        // ResourceBarrier 를 복사 상태에서 픽셀셰이더 상태로 전환한다.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Describe and create a SRV for the texture.
        // 텍스쳐에 대한 SRV Desc 를 기술하고 생성한다.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Close the command list and execute it to begin the initial GPU setup.
    // 위에서 텍스쳐 정보 업로드, 상태 전이 등의 명령들을 추가했으므로 close 로 닫아준다.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    // 커맨드 큐에서 커맨드 리스트를 실행한다.
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    // DX12 에선 fence 를 이용해 동기화를 진행한다.
    // GPU 가 설정한 지점까지 명령을 처리하면 CPU 가 그것을 알 수 있게 해준다.
    // fence 를 생성하고 어셋이 gpu 에 업로드 될 때까지 기다리는 코드이다.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        // 프레임 동기화에 사용할 이벤트 핸들을 만든다.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            // 이벤트 생성 실패 시에 처리할 코드
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        // 커맨드 리스트가 실행될 때까지 기다린다.
        // 지금은 메인 루프에서 같은 커맨드 리스트를 재사용하고 있다.
        WaitForPreviousFrame();
    }
}

std::vector<UINT8> D3D12HelloTexture::GenerateTextureData()
{
    const UINT rowPitch = TextureWidth * TexturePixelSize;
    const UINT cellPitch = rowPitch >> 3;           // The width of a cell in the checkboard texture.
    const UINT cellHeight = TextureWidth >> 3;      // The width of a cell in the checkboard texture.
    const UINT textureSize = rowPitch * TextureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += TexturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;
            pData[n + 1] = 0x00;
            pData[n + 2] = 0x00;
            pData[n + 3] = 0xff;
        }
        else
        {
            pData[n] = 0xff;
            pData[n + 1] = 0xff;
            pData[n + 2] = 0xff;
            pData[n + 3] = 0xff;
        }
    }

    return data;
}


// Update frame-based values.
void D3D12HelloTexture::OnUpdate()
{

}

// Render the scene.
void D3D12HelloTexture::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    // 장면을 렌더링 하는 데에 필요한 모든 커맨드를 기록한다.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}


void D3D12HelloTexture::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    // GPU 가 소멸자가 정리하려고 하는 리소스를 참조하지 않도록 이전 프레임이 끝날 때까지 대기한다.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}


void D3D12HelloTexture::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    // Command list allocator 는 관련 커맨드 리스트가
    // GPU 에서 실행을 완료한 경우에면 재설정 될 수 있다.
    // 앱은 fence 를 사용하여 GPU 실행 진행 상황을 확인해야 한다.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    // ExecuteCommandList() 가 특정 커맨드 리스트에서 실행되면
    // 해당 커맨드 리스트를 언제든지 다시 설정할 수 있으며, 다시 기록해야 한다.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
    // 커맨드 리스트에 필요한 상태들을 설정한다.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    // 바인딩 할 뷰포트의 수, 뷰포트의 구조체의 배열
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    // 백 버퍼가 렌더 타겟으로 사용될 것임을 나타낸다.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 렌더링에 사용될 렌더 타겟과, 뎁스 스텐실을 파이프라인에 묶는다
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    // 렌더 타겟의 개수, 렌더 타겟의 포인터, 렌더 타겟이 디스크립터에 연속적으로 저장되어 있다면 true, 뎁스 스텐실 뷰
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);


    // Record commands.
    // 커맨드 기록
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    // 렌더 타겟 색상 클리어
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    // 기본도형 위상구조 설정.
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 시작 슬롯, 정점 버퍼의 개수, 버퍼의 첫 원소를 가리키는 포인터
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    // 인덱스가 없는 정점들을 그린다.
    // 인덱스 버퍼가 있다면 drawIndexedinstnaced 함수를 써야 한다.
    // 정점의 개수, 인스턴스의 개수, 최초의 정점 인덱스, 버텍스 버퍼에서 인스턴스 당 데이터를 읽기 전에 각 인덱스에 추가된 값
    m_commandList->DrawInstanced(3, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    // 백버퍼가 present 하기 위해 사용될 것임을 나타낸다.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // 명령들을 모두 끝냈으므로 close 로 닫아준다.
    ThrowIfFailed(m_commandList->Close());
}


void D3D12HelloTexture::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // 프레임이 완료될때까지 기다리는 것은 바람직하지 않다
    // 이것은 단순하게 구현된 코드이며
    // D3D12HelloFrameBuffering 샘플은 효율적인 리소스 사용과 GPU 활용을 극대화하기 위해 Fence 를 사용하는 방법을 보여준다.

    // Signal and increment the fence value.
    // commandQueue::signal 은 GPU 측에서 fence 값을 설정하는 것이고,
    // fence::signal 은 cpu 측에서 fence 값을 설정하는 것이다.
    // 여기서는 gpu 측에서 fence 값을 설정한다.
    // gpu 가 모든 명령을 처리하기 전까지 새 fence 지점은 설정되지 않는다.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    // signal 을 호출한 되에 m_fenceValue 를 증가시킨다.
    // 이것은 CPU 의 작업이 끝났다는 의미로 GPU 가 작업하는데 필요한 렌더링 데이터가 모두 입력되었다는 뜻이다. 
    // ㄴ 이건 나중에 작업할 프레임 버퍼링에 해당되는 것이고, 이 프로젝트에는 해당하지 않음
    m_fenceValue++;

    // Wait until the previous frame is finished.
    // 이전 프레임이 끝날 때까지 기다린다.
    // GPU 가 작업을 마쳤다면 이 if 문 안으로 들어가지 않는다.
    // GetCompletedValue 함수를 통해 현재 fence 값을 가져오고, 위에서 지정한 fence 값과 비교하고 있다.
    // SetEventOnCompletion 를 통해 fence 가 특정 값에 도달할 때 발생해야 하는 이벤트를 지정한다.
    // 신호를 받을때까지 무한 대기한다.
    if (m_fence->GetCompletedValue() < fence)
    {
        // GPU 가 일을 마쳐서 gpu 가 처리하는 fence 값이 인자로 준 fence 값과 같게 되면
        // Event 값이 true 가 되어서 GPU 의 일이 끝났다는 것이 알려진다.
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // 현재의 backbuffer 의 index 를 멤버변수에 저장한다.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}