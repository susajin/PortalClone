#pragma once

#include "gameObject.h"
#include "instancingshader.h"


class Bullet : public GameObject
{
private:
	std::shared_ptr<InstancingShader> m_shader;
	std::shared_ptr<class Model> m_model;
	dx::XMFLOAT3 m_direction;
	float m_speed;

	float m_timeToLive;
	float m_timeTillStart;

public:
	Bullet() {}
	~Bullet() {}

	void Awake() override;
	void Uninit() override;
	void Update() override;
	void Draw(Pass pass) override;

	void SetDirection(dx::XMFLOAT3 direction) { m_direction = direction; }
	void SetSpeed(float speed) { m_speed = speed; }
};
