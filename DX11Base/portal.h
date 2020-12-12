#pragma once

#include "gameObject.h"
#include "portalshader.h"


class Portal : public GameObject
{
public:
	Portal() {}
	~Portal() {}

	void Awake() override;
	void Uninit() override;
	void Update() override;
	void Draw(Pass pass) override;

	void SetLookAt(dx::XMFLOAT3 lookAt) { m_lookAt = lookAt; }
	void SetUp(dx::XMFLOAT3 up) { m_up = up; }
	void SetColor(dx::XMFLOAT4 color) { m_color = color; }

	dx::XMMATRIX GetViewMatrix();
	dx::XMMATRIX GetProjectionMatrix();

private:
	std::shared_ptr<PortalShader> m_shader;
	std::shared_ptr<class Model> m_model;
	dx::XMFLOAT3 m_lookAt;
	dx::XMFLOAT3 m_up;
	dx::XMFLOAT4 m_color;
};
