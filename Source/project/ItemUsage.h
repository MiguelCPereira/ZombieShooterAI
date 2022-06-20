#pragma once
#include "Observer.h"
#include "SteeringBehaviour.h"

struct SteeringPlugin_Output;
class IExamInterface;

class ItemUsage final : public Observer
{
public:
	ItemUsage(IExamInterface* pInterface);
	~ItemUsage() override;

	void Update(float deltaTime, SteeringPlugin_Output& steering, bool& currentlyAiming);
	void OnNotify(const Event& event) override;

	void AimShot(SteeringPlugin_Output& steering, bool& currentlyAiming, float deltaTime);
	void Shoot();

	bool CollisionRayCircle(Elite::Vector2 rayOriginPoint, Elite::Vector2 rayDirection, Elite::Vector2 circleCenter, float circleRadius);

private:
	void ManageMedkits();
	void ManageFood();
	void ManagePistol(SteeringPlugin_Output& steering, bool& currentlyAiming, float deltaTime);
	
	IExamInterface* m_pInterface;
	bool m_MedkitAvailable;
	bool m_FoodAvailable;
	bool m_PistolAvailable;

	EnemyInfo m_TargetedEnemy;
	const float m_ShottingDistance;
	Face m_FaceSteering;

	bool m_ReadyToShoot;
	float m_ShootingPauseCounter;
	const float m_ShootingInterval;
};

