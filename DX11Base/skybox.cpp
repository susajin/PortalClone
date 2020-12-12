#include "pch.h"
#include "manager.h"
#include "renderer.h"
#include "modelmanager.h"
#include "skybox.h"
#include "math.h"
#include "input.h"
#include "player.h"
#include "light.h"


void Skybox::Init()
{
	GameObject::Init();

	// init the shader
	m_shader = CRenderer::GetShader<BasicLightShader>();

	// init the skybox
	ModelManager::GetModel(MODEL_SKYBOX, m_model);

	m_position = dx::XMFLOAT3(0.0F, 0.0F, 0.0F);
	m_rotation = dx::XMFLOAT3(0.0F, 0.0F, 0.0F);
	m_scale = dx::XMFLOAT3(500.0F, 500.0F, 500.0F);
}

void Skybox::Uninit()
{
	GameObject::Uninit();
}

void Skybox::Update()
{
	GameObject::Update();
}

void Skybox::Draw(Pass pass)
{
	if (pass == Pass::Default)
	{
		GameObject::Draw(pass);

		// set the world matrix for this object
		dx::XMMATRIX world = GetWorldMatrix();
		m_shader->SetWorldMatrix(&world);

		// draw the model
		CRenderer::DrawModel(m_shader, m_model);
	}
}
