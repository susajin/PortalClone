#include "pch.h"
#include <io.h>
#include <typeinfo>
#include "main.h"
#include "model.h"
#include "renderer.h"
#include "basiclightshader.h"
#include "uishader.h"
#include "lineshader.h"
#include "rangeshader.h"
#include "minimapshader.h"
#include "writedepthshader.h"
#include "skinningcs.h"

// for hlsl debugging
#ifdef _DEBUG
#define DEVICE_DEBUG D3D11_CREATE_DEVICE_DEBUG
#else
#define DEVICE_DEBUG 0
#endif


D3D_FEATURE_LEVEL       CRenderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

ID3D11Device*           CRenderer::m_D3DDevice = nullptr;
ID3D11DeviceContext*    CRenderer::m_ImmediateContext = nullptr;
IDXGISwapChain*         CRenderer::m_SwapChain = nullptr;
ID3D11RenderTargetView* CRenderer::m_RenderTargetView = nullptr;
ID3D11DepthStencilView* CRenderer::m_DepthStencilView = nullptr;

ID3D11DepthStencilState* CRenderer::m_DepthStateEnable = nullptr;
ID3D11DepthStencilState* CRenderer::m_DepthStateDisable = nullptr;

ID3D11RasterizerState* CRenderer::m_rasterizerCullBack = nullptr;
ID3D11RasterizerState* CRenderer::m_rasterizerCullFront = nullptr;
ID3D11RasterizerState* CRenderer::m_rasterizerWireframe = nullptr;

std::map<UINT, ID3D11RenderTargetView*> CRenderer::m_renderTargetViews;

std::vector<std::shared_ptr<Shader>> CRenderer::m_shaders = std::vector<std::shared_ptr<Shader>>();
std::vector<std::shared_ptr<ComputeShader>> CRenderer::m_computeShaders = std::vector<std::shared_ptr<ComputeShader>>();

std::weak_ptr<Shader> CRenderer::m_activeShader;


void CRenderer::Init()
{
	HRESULT hr = S_OK;
	
	// デバイス、スワップチェーン、コンテキスト生成
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = SCREEN_WIDTH;
	sd.BufferDesc.Height = SCREEN_HEIGHT;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetWindow();
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	hr = D3D11CreateDeviceAndSwapChain( NULL,
										D3D_DRIVER_TYPE_HARDWARE,
										NULL,
										DEVICE_DEBUG,
										NULL,
										0,
										D3D11_SDK_VERSION,
										&sd,
										&m_SwapChain,
										&m_D3DDevice,
										&m_FeatureLevel,
										&m_ImmediateContext );


	// レンダーターゲットビュー生成、設定
	ID3D11Texture2D* pBackBuffer = NULL;
	m_SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
	m_D3DDevice->CreateRenderTargetView( pBackBuffer, NULL, &m_RenderTargetView );
	pBackBuffer->Release();

	//ステンシル用テクスチャー作成
	ID3D11Texture2D* depthTexture = NULL;
	D3D11_TEXTURE2D_DESC td;
	ZeroMemory( &td, sizeof(td) );
	td.Width			= sd.BufferDesc.Width;
	td.Height			= sd.BufferDesc.Height;
	td.MipLevels		= 1;
	td.ArraySize		= 1;
	td.Format			= DXGI_FORMAT_D24_UNORM_S8_UINT;
	td.SampleDesc		= sd.SampleDesc;
	td.Usage			= D3D11_USAGE_DEFAULT;
	td.BindFlags		= D3D11_BIND_DEPTH_STENCIL;
    td.CPUAccessFlags	= 0;
    td.MiscFlags		= 0;
	m_D3DDevice->CreateTexture2D( &td, NULL, &depthTexture );

	//ステンシルターゲット作成
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
	ZeroMemory( &dsvd, sizeof(dsvd) );
	dsvd.Format			= td.Format;
	dsvd.ViewDimension	= D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvd.Flags			= 0;
	m_D3DDevice->CreateDepthStencilView( depthTexture, &dsvd, &m_DepthStencilView );

	m_ImmediateContext->OMSetRenderTargets( 1, &m_RenderTargetView, m_DepthStencilView );
	m_renderTargetViews[1] = m_RenderTargetView;

	// ビューポート設定
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)SCREEN_WIDTH;
	vp.Height = (FLOAT)SCREEN_HEIGHT;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_ImmediateContext->RSSetViewports( 1, &vp );

	// create backface culling rasterizer state
	D3D11_RASTERIZER_DESC rd; 
	ZeroMemory( &rd, sizeof( rd ) );
	rd.FillMode = D3D11_FILL_SOLID; 
	rd.CullMode = D3D11_CULL_BACK; 
	rd.DepthClipEnable = TRUE; 
	rd.MultisampleEnable = FALSE; 

	m_D3DDevice->CreateRasterizerState( &rd, &m_rasterizerCullBack);

	// create frontface culling rasterizer state
	ZeroMemory(&rd, sizeof(rd));
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_FRONT;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;

	m_D3DDevice->CreateRasterizerState(&rd, &m_rasterizerCullFront);

	// create rasterizer with wireframe mode
	ZeroMemory(&rd, sizeof(rd));
	rd.FillMode = D3D11_FILL_WIREFRAME;
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;

	m_D3DDevice->CreateRasterizerState(&rd, &m_rasterizerWireframe);

	// set the rasterizer state
	SetRasterizerState(RasterizerState_CullBack);

	// ブレンドステート設定
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory( &blendDesc, sizeof( blendDesc ) );
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	ID3D11BlendState* blendState = NULL;
	m_D3DDevice->CreateBlendState( &blendDesc, &blendState );
	m_ImmediateContext->OMSetBlendState( blendState, blendFactor, 0xffffffff );

	// 深度ステンシルステート設定
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory( &depthStencilDesc, sizeof( depthStencilDesc ) );
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask	= D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	m_D3DDevice->CreateDepthStencilState( &depthStencilDesc, &m_DepthStateEnable );//深度有効ステート
	m_ImmediateContext->OMSetDepthStencilState( m_DepthStateEnable, NULL );

	// init the shaders
	m_shaders.emplace_back(std::shared_ptr<BasicLightShader>(new BasicLightShader()));
	m_shaders.back()->Init();

	m_shaders.emplace_back(std::shared_ptr<UIShader>(new UIShader()));
	m_shaders.back()->Init();

	m_shaders.emplace_back(std::shared_ptr<LineShader>(new LineShader()));
	m_shaders.back()->Init();

	m_shaders.emplace_back(std::shared_ptr<RangeShader>(new RangeShader()));
	m_shaders.back()->Init();

	m_shaders.emplace_back(std::shared_ptr<MinimapShader>(new MinimapShader()));
	m_shaders.back()->Init();

	m_shaders.emplace_back(std::shared_ptr<WriteDepthShader>(new WriteDepthShader()));
	m_shaders.back()->Init();

	// set the active shader
	SetShader(m_shaders.front());

	// init compute shaders
	m_computeShaders.emplace_back(std::shared_ptr<SkinningCompute>(new SkinningCompute()));
	m_computeShaders.back()->Init();
}

void CRenderer::Uninit()
{
	m_ImmediateContext->ClearState();
	m_RenderTargetView->Release();
	m_SwapChain->Release();
	m_ImmediateContext->Release();
	m_D3DDevice->Release();

	for (auto shader : m_shaders)
		shader->Uninit();

	m_shaders.clear();

	for (auto shader : m_computeShaders)
		shader->Uninit();

	m_computeShaders.clear();
}

void CRenderer::Begin(UINT renderPass)
{
	float ClearColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };
	ID3D11RenderTargetView* renderTarget = m_renderTargetViews[renderPass];

	// set the render target
	m_ImmediateContext->OMSetRenderTargets(1, &renderTarget, m_DepthStencilView);

	// clear the render target buffer
	m_ImmediateContext->ClearRenderTargetView(renderTarget, ClearColor);
	m_ImmediateContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void CRenderer::End()
{
	m_SwapChain->Present( 1, 0 );
}

void CRenderer::SetDepthEnable( bool Enable )
{
	if( Enable )
		m_ImmediateContext->OMSetDepthStencilState( m_DepthStateEnable, NULL );
	else
		m_ImmediateContext->OMSetDepthStencilState( m_DepthStateDisable, NULL );

}

void CRenderer::SetShader(const std::shared_ptr<Shader>& shader)
{
	// return if the active shader is the same
	if (auto activeShader = m_activeShader.lock())
	{
		if (typeid(*shader) == typeid(*activeShader))
			return;
	}

	m_activeShader = shader;

	// シェーダ設定
	m_ImmediateContext->VSSetShader(shader->m_vertexShader, NULL, 0);
	m_ImmediateContext->PSSetShader(shader->m_pixelShader, NULL, 0);

	shader->UpdateConstantBuffers();
}

void CRenderer::SetRasterizerState(RasterizerState state)
{
	switch (state)
	{
	case RasterizerState_CullBack:
		m_ImmediateContext->RSSetState(m_rasterizerCullBack);
		break;
	case RasterizerState_CullFront:
		m_ImmediateContext->RSSetState(m_rasterizerCullFront);
		break;
	case RasterizerState_Wireframe:
		m_ImmediateContext->RSSetState(m_rasterizerWireframe);
		break;
	default:
		break;
	}
}

void CRenderer::DrawLine(const std::shared_ptr<Shader> shader, ID3D11Buffer** vertexBuffer, UINT vertexCount)
{
	// set the active shader
	SetShader(shader);

	// set vertex buffer
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	CRenderer::GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffer, &stride, &offset);

	//プリミティブトポロジー設定
	CRenderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	//ポリゴン描画
	CRenderer::GetDeviceContext()->Draw(vertexCount, 0);
}

void CRenderer::DrawPolygon(const std::shared_ptr<Shader> shader, ID3D11Buffer** vertexBuffer, UINT vertexCount)
{
	// set the active shader
	SetShader(shader);

	// set vertex buffer
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	CRenderer::GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffer, &stride, &offset);

	//プリミティブトポロジー設定
	CRenderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//ポリゴン描画
	CRenderer::GetDeviceContext()->Draw(vertexCount, 0);
}

void CRenderer::DrawPolygonIndexed(const std::shared_ptr<Shader> shader, ID3D11Buffer** vertexBuffer, ID3D11Buffer* indexBuffer, UINT indexCount)
{
	// set the active shader
	SetShader(shader);

	// set vertex and index buffers
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	CRenderer::GetDeviceContext()->IASetVertexBuffers(0, 1, vertexBuffer, &stride, &offset);
	CRenderer::GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	//プリミティブトポロジー設定
	CRenderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//ポリゴン描画
	CRenderer::GetDeviceContext()->DrawIndexed(indexCount, 0, 0);
}

void CRenderer::DrawModel(const std::shared_ptr<Shader> shader, const std::shared_ptr<Model> model)
{
	// set the active shader
	SetShader(shader);

	// set material
	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = dx::XMFLOAT4(1, 1, 1, 1);
	material.Ambient = dx::XMFLOAT4(1, 1, 1, 1);
	shader->SetMaterial(material);

	//プリミティブトポロジー設定
	CRenderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// loop for every mesh, set the corresponding textures and draw the model
	for (unsigned int m = 0; m < model->m_scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = model->m_scene->mMeshes[m];

		// set texture
		aiMaterial* material = model->m_scene->mMaterials[mesh->mMaterialIndex];

		aiString path;
		material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		shader->PS_SetTexture(model->m_texture[path.data]);

		// set vertex buffer
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		CRenderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &model->m_vertexBuffer[m], &stride, &offset);

		// set index buffer
		CRenderer::GetDeviceContext()->IASetIndexBuffer(model->m_indexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// draw
		CRenderer::GetDeviceContext()->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
	}
}

void CRenderer::BindRenderTargetView(ID3D11RenderTargetView* renderTargetView, UINT renderPass)
{
	m_renderTargetViews[renderPass] = renderTargetView;
}
