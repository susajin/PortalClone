#include "pch.h"
#include "manager.h"
#include "renderer.h"
#include "modelmanager.h"
#include "player.h"
#include "math.h"
#include "input.h"
#include "main.h"
#include "stage.h"
#include "terrain.h"
#include "light.h"
#include "rendertexture.h"
#include "fpscamera.h"
#include "cube.h"


void Player::Awake()
{
	GameObject::Awake();

	// init player stuff
	m_shader = CRenderer::GetShader<BasicLightShader>();

	ModelManager::GetModel(MODEL_PLAYER, m_model);

	m_position = dx::XMFLOAT3(0.0F, 0.0F, 0.0F);
	m_rotation = dx::XMFLOAT3(0.0F, 0.0F, 0.0F);
	m_scale = dx::XMFLOAT3(0.06F, 0.06F, 0.06F);

	virtualUp = { 0, 1, 0 };
	m_obb.Init((GameObject*)this, 40, 70, 40, 0, 35, 0);

	m_moveSpeed = 0.3F;
	m_titleDisplay = false;
	m_enableFrustumCulling = false;
	m_isJumping = false;
	m_velocity = { 0,0,0 };

	m_model->Update(0, 0);
}

void Player::Init()
{
	GameObject::Init();
}

void Player::Uninit()
{
	GameObject::Uninit();
}

void Player::Update()
{
	GameObject::Update();

	//static float frame = 0;
	//frame += 0.2F;

	// in title display mode
	if (m_titleDisplay)
	{
		//m_model->Update(frame, 0);
		return;
	}

	// animation
	//else if (CInput::GetKeyPress(DIK_W) || CInput::GetKeyPress(DIK_A) || CInput::GetKeyPress(DIK_S) || CInput::GetKeyPress(DIK_D))
	//	m_model->Update(frame, 1);
	//else
	//	m_model->Update(frame, 0);

	// movement, jump
	Movement();
	Jump();

	// apply gravity
	m_velocity.y -= 0.098f;

	// update position
	if (m_velocity.y < -1.0f)
		m_velocity.y = -1.0f;
	m_position += m_velocity;

	// collision
	dx::XMFLOAT3 intersection = { 0,0,0 };

	// cube collision
	m_obb.Update();
	auto cube = CManager::GetActiveScene()->GetGameObjects<Cube>(0).front();
	intersection += Collision::ObbObbCollision(&m_obb, cube->GetObb());
	
	//// stage collision
	auto stageColliders = CManager::GetActiveScene()->GetGameObjects<Stage>(0).front()->GetColliders();
	for (const auto& col : *stageColliders)
	{
		// ignore collision on walls attached to the current colliding portal
		if (m_entrancePortal.lock())
		{
			if (PortalManager::GetPortal(PortalType::Blue)->GetAttachedColliderId() == col->GetId() || 
				PortalManager::GetPortal(PortalType::Orange)->GetAttachedColliderId() == col->GetId())
				continue;
		}
	
			intersection += Collision::ObbPolygonCollision(&m_obb, col);
	}

	// apply collision offset
	m_position += intersection;

	// check if player landed on something
	if (intersection.y != 0)
	{
		m_velocity.y = 0;
		m_isJumping = false;
	}

	// adjust player virtual up position back to 0,1,0
	if (virtualUp.x != 0 || virtualUp.y != 1 || virtualUp.z != 0)
		virtualUp = Lerp(virtualUp, dx::XMFLOAT3{ 0,1,0 }, 0.06f);

	// shoot portal
	if (CInput::GetMouseLeftTrigger())
		ShootPortal(PortalType::Blue);
	else if (CInput::GetMouseRightTrigger())
		ShootPortal(PortalType::Orange);
}

void Player::Draw(Pass pass)
{
	GameObject::Draw(pass);
	m_obb.Draw();

	// title rendering
	if (m_titleDisplay)
	{
		dx::XMMATRIX world = GameObject::GetWorldMatrix();
		m_shader->SetWorldMatrix(&world);

		m_shader->SetDirectionalLight(LightManager::GetDirectionalLight());

		MATERIAL material;
		ZeroMemory(&material, sizeof(material));
		material.Diffuse = dx::XMFLOAT4(1, 1, 1, 1);
		m_shader->SetMaterial(material);

		auto renderTex = CRenderer::GetRenderTexture(2);
		if (renderTex)
		{
			m_shader->SetShadowMapTexture(CRenderer::GetRenderTexture(2)->GetRenderTexture());
			m_shader->SetLightProjectionMatrix(&LightManager::GetDirectionalProjectionMatrix());
			m_shader->SetLightViewMatrix(&LightManager::GetDirectionalViewMatrix());
		}

		// draw the model
		CRenderer::DrawModel(m_shader, m_model);
		return;
	}

	// set buffers
	m_shader->SetDirectionalLight(LightManager::GetDirectionalLight());

	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = dx::XMFLOAT4(1, 1, 1, 1);
	m_shader->SetMaterial(material);

	m_shader->SetShadowMapTexture(CRenderer::GetRenderTexture(2)->GetRenderTexture());
	m_shader->SetLightProjectionMatrix(&LightManager::GetDirectionalProjectionMatrix());
	m_shader->SetLightViewMatrix(&LightManager::GetDirectionalViewMatrix());

	if (pass == Pass::PortalBlue)
	{
		m_shader->SetViewMatrix(&PortalManager::GetViewMatrix(PortalType::Blue));
		m_shader->SetProjectionMatrix(&PortalManager::GetProjectionMatrix(PortalType::Blue));
	}
	else if (pass == Pass::PortalOrange)
	{
		m_shader->SetViewMatrix(&PortalManager::GetViewMatrix(PortalType::Orange));
		m_shader->SetProjectionMatrix(&PortalManager::GetProjectionMatrix(PortalType::Orange));
	}

	// only draw player through portal view
	if ((pass == Pass::PortalBlue || pass == Pass::PortalOrange))
	{
		dx::XMMATRIX world = GetAdjustedWorldMatrix();
		m_shader->SetWorldMatrix(&world);
		CRenderer::DrawModel(m_shader, m_model);
	}

	// draw cloned player
	if (m_entrancePortal.lock())
	{
		dx::XMMATRIX world = GetClonedWorldMatrix();
		m_shader->SetWorldMatrix(&world);
		//CRenderer::DrawModel(m_shader, m_model);

		ImGui::SetNextWindowSize(ImVec2(350, 200));
		ImGui::Begin("Player Debug");
		ImGui::Text("Entrance Portal");
		const char* a = m_entrancePortal.lock()->GetType() == PortalType::Blue ? "Blue" : "Orange";
		ImGui::Text("%s", a);
		ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f", m_clonedForward.x, m_clonedForward.y, m_clonedForward.z);
		ImGui::End();
	}
}

void Player::Draw(const std::shared_ptr<Shader>& shader, Pass pass)
{
	if (pass == Pass::StencilOnly)
		return;

	GameObject::Draw(shader, pass);

	// set shader buffers
	dx::XMMATRIX world = GetAdjustedWorldMatrix();
	shader->SetWorldMatrix(&world);

	shader->SetProjectionMatrix(&LightManager::GetDirectionalProjectionMatrix());
	shader->SetViewMatrix(&LightManager::GetDirectionalViewMatrix());

	// draw the model
	CRenderer::DrawModel(shader, m_model);
}

void Player::SwapPosition()
{
	// update player
	SetPosition(m_clonedPos);
	virtualUp = m_clonedUp;
	dx::XMFLOAT3 newVel = { m_clonedForward.x * m_velocity.x, m_clonedForward.y * m_velocity.x, m_clonedForward.z * m_velocity.x };
	newVel += dx::XMFLOAT3(m_clonedForward.x * m_velocity.y, m_clonedForward.y * m_velocity.y, m_clonedForward.z * m_velocity.y);
	newVel += dx::XMFLOAT3(m_clonedForward.x * m_velocity.z, m_clonedForward.y * m_velocity.z, m_clonedForward.z * m_velocity.z);
	//m_velocity = newVel;

	// update main camera
	auto cam = std::static_pointer_cast<FPSCamera>(CManager::GetActiveScene()->GetMainCamera());
	cam->Swap(m_clonedForward);
}

void Player::Movement()
{
	auto forward = std::static_pointer_cast<FPSCamera>(CManager::GetActiveScene()->GetMainCamera())->GetForwardVector();
	auto right = std::static_pointer_cast<FPSCamera>(CManager::GetActiveScene()->GetMainCamera())->GetRightVector();
	forward = dx::XMVectorSetY(forward, 0);
	right = dx::XMVectorSetY(right, 0);

	// normalized wasd movement
	dx::XMVECTOR moveDirection = dx::XMVectorZero();
	if (CInput::GetKeyPress(DIK_W))
		moveDirection = dx::XMVectorAdd(moveDirection, forward);
	if (CInput::GetKeyPress(DIK_A))
		moveDirection = dx::XMVectorSubtract(moveDirection, right);
	if (CInput::GetKeyPress(DIK_S))
		moveDirection = dx::XMVectorSubtract(moveDirection, forward);
	if (CInput::GetKeyPress(DIK_D))
		moveDirection = dx::XMVectorAdd(moveDirection, right);

	moveDirection = dx::XMVector3Normalize(moveDirection);

	dx::XMFLOAT3 tempVel;
	dx::XMStoreFloat3(&tempVel, dx::XMVectorScale(moveDirection, m_moveSpeed));
	m_velocity.x = tempVel.x;
	m_velocity.z = tempVel.z;
}

void Player::Jump()
{
	// add y velocity on jump
	if (!m_isJumping && CInput::GetKeyTrigger(DIK_SPACE))
	{
		m_isJumping = true;
		m_velocity.y += 1.4f;
	}
}

void Player::ShootPortal(PortalType type)
{
	auto cam = std::static_pointer_cast<FPSCamera>(CManager::GetActiveScene()->GetMainCamera());
	if (cam->InDebugMode())
		return;

	auto stage = CManager::GetActiveScene()->GetGameObjects<Stage>(0).front();

	dx::XMFLOAT3 point, direction;
	dx::XMStoreFloat3(&point, cam->GetPosition());
	dx::XMStoreFloat3(&direction, cam->GetForwardVector());

	dx::XMFLOAT3 pos, normal, up;
	auto colliders = stage->GetColliders();
	for (const auto& collider : *colliders)
	{
		if(Collision::LinePolygonCollision(collider, point, direction, pos, normal, up))
		{
			PortalManager::CreatePortal(type, pos, normal, up, collider->GetId());
			break;
		}
	}
}

dx::XMMATRIX Player::GetAdjustedWorldMatrix() const
{
	auto right = std::static_pointer_cast<FPSCamera>(CManager::GetActiveScene()->GetMainCamera())->GetRightVector();
	dx::XMMATRIX world = GetWorldMatrix();
	dx::XMFLOAT4X4 t;
	dx::XMStoreFloat4x4(&t, world);

	dx::XMVECTOR up = dx::XMVectorSet(0, 1, 0, 0);
	dx::XMVECTOR xaxis = dx::XMVector3Normalize(right);
	dx::XMVECTOR zaxis = dx::XMVector3Normalize(dx::XMVector3Cross(xaxis, up));
	dx::XMVECTOR yaxis = dx::XMVector3Cross(zaxis, xaxis);

	dx::XMFLOAT3 z, x, y;
	dx::XMStoreFloat3(&z, zaxis);
	dx::XMStoreFloat3(&x, xaxis);
	dx::XMStoreFloat3(&y, yaxis);

	t._11 = x.x * m_scale.x;
	t._12 = x.y * m_scale.x;
	t._13 = x.z * m_scale.x;
	t._21 = y.x * m_scale.y;
	t._22 = y.y * m_scale.y;
	t._23 = y.z * m_scale.y;
	t._31 = z.x * m_scale.z;
	t._32 = z.y * m_scale.z;
	t._33 = z.z * m_scale.z;

	return dx::XMLoadFloat4x4(&t);
}

dx::XMMATRIX Player::GetClonedWorldMatrix()
{
	if (auto portal = m_entrancePortal.lock())
	{
		// update the cloned orientation for swapping
		dx::XMMATRIX matrix = portal->GetPlayerOrientationMatrix(m_position);
		dx::XMFLOAT4X4 t;
		dx::XMStoreFloat4x4(&t, matrix);

		m_clonedPos.x = t._41;
		m_clonedPos.y = t._42;
		m_clonedPos.z = t._43;
		m_clonedForward.x = t._31;
		m_clonedForward.y = t._32;
		m_clonedForward.z = t._33;

		// return the cloned world matrix
		matrix = portal->GetPlayerWorldMatrix(m_position);
		dx::XMStoreFloat4x4(&t, matrix);

		m_clonedUp.x = t._21;
		m_clonedUp.y = t._22;
		m_clonedUp.z = t._23;

		t._11 *= m_scale.x;
		t._12 *= m_scale.x;
		t._13 *= m_scale.x;
		t._21 *= m_scale.y;
		t._22 *= m_scale.y;
		t._23 *= m_scale.y;
		t._31 *= m_scale.z;
		t._32 *= m_scale.z;
		t._33 *= m_scale.z;

		return dx::XMLoadFloat4x4(&t);
	}

	return dx::XMMatrixIdentity();
}

dx::XMMATRIX Player::GetWorldMatrix() const
{
	auto right = std::static_pointer_cast<FPSCamera>(CManager::GetActiveScene()->GetMainCamera())->GetRightVector();
	dx::XMMATRIX world = GameObject::GetWorldMatrix();
	dx::XMFLOAT4X4 t;
	dx::XMStoreFloat4x4(&t, world);

	dx::XMVECTOR yaxis = dx::XMVector3Normalize(dx::XMLoadFloat3(&virtualUp));
	dx::XMVECTOR xaxis = dx::XMVector3Normalize(right);
	dx::XMVECTOR zaxis = dx::XMVector3Normalize(dx::XMVector3Cross(xaxis, yaxis));

	dx::XMFLOAT3 z, x, y;
	dx::XMStoreFloat3(&z, zaxis);
	dx::XMStoreFloat3(&x, xaxis);
	dx::XMStoreFloat3(&y, yaxis);

	t._11 = x.x * m_scale.x;
	t._12 = x.y * m_scale.x;
	t._13 = x.z * m_scale.x;
	t._21 = y.x * m_scale.y;
	t._22 = y.y * m_scale.y;
	t._23 = y.z * m_scale.y;
	t._31 = z.x * m_scale.z;
	t._32 = z.y * m_scale.z;
	t._33 = z.z * m_scale.z;

	return dx::XMLoadFloat4x4(&t);
}
