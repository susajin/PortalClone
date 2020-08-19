#pragma once

#include "shader.h"


class RangeShader : public Shader
{
public:
	void Init() override;
	void Uninit() override;

	void UpdateConstantBuffers() override
	{
		auto deviceContext = CRenderer::GetDeviceContext();

		deviceContext->VSSetConstantBuffers(0, 1, &m_worldBuffer);
		deviceContext->VSSetConstantBuffers(1, 1, &m_viewBuffer);
		deviceContext->VSSetConstantBuffers(2, 1, &m_projectionBuffer);
		deviceContext->VSSetConstantBuffers(3, 1, &m_materialBuffer);
		deviceContext->VSSetConstantBuffers(4, 1, &m_lightBuffer);

		deviceContext->PSSetConstantBuffers(0, 1, &m_rangeBuffer);
	}

	void PS_SetRangeBuffer(const float range, const dx::XMFLOAT3& playerPos)
	{
		Range r;
		r.range = range;
		r.playerPos[0] = playerPos.x;
		r.playerPos[1] = playerPos.y;
		r.playerPos[2] = playerPos.z;

		CRenderer::GetDeviceContext()->UpdateSubresource(m_rangeBuffer, 0, NULL, &r, 0, 0);
	}

	void SetWorldMatrix(dx::XMMATRIX *WorldMatrix) override
	{
		dx::XMMATRIX world = *WorldMatrix;
		world = dx::XMMatrixTranspose(world);
		CRenderer::GetDeviceContext()->UpdateSubresource(m_worldBuffer, 0, NULL, &world, 0, 0);
	}

	void SetViewMatrix(dx::XMMATRIX *ViewMatrix) override
	{
		dx::XMMATRIX view = *ViewMatrix;
		view = dx::XMMatrixTranspose(view);
		CRenderer::GetDeviceContext()->UpdateSubresource(m_viewBuffer, 0, NULL, &view, 0, 0);
	}

	void SetProjectionMatrix(dx::XMMATRIX *ProjectionMatrix) override
	{
		dx::XMMATRIX projection = *ProjectionMatrix;
		projection = dx::XMMatrixTranspose(projection);
		CRenderer::GetDeviceContext()->UpdateSubresource(m_projectionBuffer, 0, NULL, &projection, 0, 0);
	}

	void SetMaterial(MATERIAL Material) override
	{
		CRenderer::GetDeviceContext()->UpdateSubresource(m_materialBuffer, 0, NULL, &Material, 0, 0);
	}

	void SetLight(LIGHT Light) override
	{
		CRenderer::GetDeviceContext()->UpdateSubresource(m_lightBuffer, 0, NULL, &Light, 0, 0);
	}

	void PS_SetTexture(ID3D11ShaderResourceView* texture) override
	{
		CRenderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);
	}

private:
	ID3D11Buffer* m_rangeBuffer;

	struct Range
	{
		float range;
		float playerPos[3];
	};
};