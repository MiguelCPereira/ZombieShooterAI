#include "stdafx.h"
#include "ItemUsage.h"
#include <IExamInterface.h>

ItemUsage::ItemUsage(IExamInterface* pInterface)
	: m_pInterface(pInterface)
	, m_MedkitAvailable(false)
	, m_FoodAvailable(false)
	, m_PistolAvailable(false)
	, m_TargetedEnemy()
	, m_ShottingDistance(12.f)
	, m_FaceSteering()
	, m_ReadyToShoot(true)
	, m_ShootingPauseCounter(0.f)
	, m_ShootingInterval(0.2f)
{
}

ItemUsage::~ItemUsage()
{
	m_pInterface = nullptr;
}


void ItemUsage::Update(float deltaTime, SteeringPlugin_Output& steering, bool& currentlyAiming)
{
	ManageMedkits();
	ManageFood();
	ManagePistol(steering, currentlyAiming, deltaTime);
}

void ItemUsage::ManageMedkits()
{
	const auto agentMaxHP = 10.f;
	
	// Check if the agent needs healing
	const auto agentInfo = m_pInterface->Agent_GetInfo();
	if (agentInfo.Health < agentMaxHP - 1.f)
	{
		// Check if there are medkits in the inventory
		if (m_MedkitAvailable)
		{
			// Change the member variable bool preemptively
			// (it will be changed back to true if, after using this one medkit, more medkits remain in the inventory)
			m_MedkitAvailable = false;
			
			// Go through each slot in the inventory
			bool firstMedkitFound = false;
			const int nrOfSlots = m_pInterface->Inventory_GetCapacity();
			for (auto currentSlot = 0; currentSlot < nrOfSlots; currentSlot++)
			{
				// If a slot has an item
				ItemInfo itemInSlot;
				if (m_pInterface->Inventory_GetItem(currentSlot, itemInSlot))
				{
					// If that item is a medkit
					if (itemInSlot.Type == eItemType::MEDKIT)
					{
						// If the agent has not been healed yet
						if (firstMedkitFound == false)
						{
							// Try to heal up (will only work if the medkit has enough units)
							if (m_pInterface->Inventory_UseItem(currentSlot) == false)
							{
								// If it fails, throw the empty medkit away
								m_pInterface->Inventory_RemoveItem(currentSlot);
							}
							else // If it works
							{
								// If it was the last unit in the medkit, throw it away
								if(m_pInterface->Medkit_GetHealth(itemInSlot) == 0)
									m_pInterface->Inventory_RemoveItem(currentSlot);
								
								firstMedkitFound = true; // Change the bool so the agent won't be healed again in this same Update(), in case there are more medkits in the inventory
							}
						}
						else
						{
							m_MedkitAvailable = true;
							break;
						}
					}
				}
			}
		}
	}
}

void ItemUsage::ManageFood()
{
	const auto agentMaxEnergy = 10.f;

	// Check if the agent needs energy
	const auto agentInfo = m_pInterface->Agent_GetInfo();
	if (agentInfo.Energy < agentMaxEnergy - 1.f)
	{
		// Check if there is food in the inventory
		if (m_FoodAvailable)
		{
			// Change the member variable bool preemptively
			// (it will be changed back to true if, after using this one instance of food, more food remains in the inventory)
			m_FoodAvailable = false;

			// Go through each slot in the inventory
			bool firstFoodFound = false;
			const int nrOfSlots = m_pInterface->Inventory_GetCapacity();
			for (auto currentSlot = 0; currentSlot < nrOfSlots; currentSlot++)
			{
				// If a slot has an item
				ItemInfo itemInSlot;
				if (m_pInterface->Inventory_GetItem(currentSlot, itemInSlot))
				{
					// If that item is food
					if (itemInSlot.Type == eItemType::FOOD)
					{
						// If the agent has not eaten yet
						if (firstFoodFound == false)
						{
							// Try to eat (will only work if the food has enough units)
							if (m_pInterface->Inventory_UseItem(currentSlot) == false)
							{
								// If it fails, throw the empty food item away
								m_pInterface->Inventory_RemoveItem(currentSlot);
							}
							else // If it works
							{
								// If it was the last unit of energy in the food, throw it away
								if (m_pInterface->Food_GetEnergy(itemInSlot) == 0)
									m_pInterface->Inventory_RemoveItem(currentSlot);
								
								firstFoodFound = true; // Change the bool so the agent won't eat again in this same Update(), in case there is more food in the inventory
							}
						}
						else
						{
							m_FoodAvailable = true;
							break;
						}
					}
				}
			}
		}
	}
}

void ItemUsage::ManagePistol(SteeringPlugin_Output& steering, bool& currentlyAiming, float deltaTime)
{
	// Preventively change the bool (which will be changed back to true in case the AimShot() function runs)
	currentlyAiming = false;

	// Continue the pause timer, in case the agent just shot
	if(m_ReadyToShoot == false)
	{
		m_ShootingPauseCounter += deltaTime;
		if (m_ShootingPauseCounter >= m_ShootingInterval)
		{
			m_ShootingPauseCounter = 0.f;
			m_ReadyToShoot = true;
		}
	}
	
	// If the agent has a loaded pistol
	if (m_PistolAvailable)
	{
		const auto agentInfo = m_pInterface->Agent_GetInfo();

		// Store in a vector all enemies in the FOV
		vector<EnemyInfo> enemiesInFOV{};
		for (int i = 0;; ++i)
		{
			EntityInfo ei = {};
			if (m_pInterface->Fov_GetEntityByIndex(i, ei))
			{
				if (ei.Type == eEntityType::ENEMY)
				{
					EnemyInfo enemyInfo{};
					if (m_pInterface->Enemy_GetInfo(ei, enemyInfo))
					{
						enemiesInFOV.push_back(enemyInfo);
					}
				}

				continue;
			}
			break;
		}

		// If any enemy is spotted, target the closest
		if (enemiesInFOV.empty() == false)
		{
			m_TargetedEnemy = enemiesInFOV[0];
			for (const auto& enemy : enemiesInFOV)
			{
				if(enemy.Location.Distance(agentInfo.Position) < m_TargetedEnemy.Location.Distance(agentInfo.Position))
					m_TargetedEnemy = enemy;
			}
			
			AimShot(steering, currentlyAiming, deltaTime);
		}
	}
}

void ItemUsage::OnNotify(const Event& event)
{
	switch (event)
	{
	case Event::MedkitPickedUp:
		m_MedkitAvailable = true;
		break;

	case Event::FoodPickedUp:
		m_FoodAvailable = true;
		break;

	case Event::PistolPickedUp:
		m_PistolAvailable = true;
		break;
	default:
		break;
	}
}

void ItemUsage::AimShot(SteeringPlugin_Output& steering, bool& currentlyAiming, float deltaTime)
{
	const auto agentInfo = m_pInterface->Agent_GetInfo();
	
	// Face the zombie
	currentlyAiming = true;
	m_FaceSteering.SetTarget(m_TargetedEnemy.Location);

	// If the zombie is inside the shooting range
	if (m_TargetedEnemy.Location.Distance(agentInfo.Position) < m_ShottingDistance)
	{
		// If not waiting between shots. The waiting exists duo to rare shot misses that can happen if the targeted enemy gets suddenly stuck on something (in this case, this pause will stop the agent from wasting bullets)
		if (m_ReadyToShoot)
		{
			// Calculate the agent's current rotation
			double currentRotation = double(agentInfo.Orientation);
			if (currentRotation > 6.)
				currentRotation -= 6. * (int(currentRotation) / 6);

			if (currentRotation < 0.)
				currentRotation += 6. * (int(-currentRotation) / 6 + 1);

			float currentRotationAngle;
			if (currentRotation != 0.)
			{
				currentRotationAngle = 360. * currentRotation / 6. - 90.;
				if (currentRotationAngle < 0.)
					currentRotationAngle += 360.;
			}
			else currentRotationAngle = 270.;

			const Elite::Vector2 currentDirection{ cos(Elite::ToRadians(currentRotationAngle)), sin(Elite::ToRadians(currentRotationAngle)) };


			// Predict the enemy's position
			const float predictionTime = m_TargetedEnemy.Location.Distance(agentInfo.Position) / m_TargetedEnemy.LinearVelocity.Magnitude(); // Calculate the prediction time by dividing the distance with the enemy's current speed
			const Elite::Vector2 targetDirection = m_TargetedEnemy.LinearVelocity.GetNormalized(); // Get the target's normalized direction vector
			const auto predictedTarget = m_TargetedEnemy.Location + (targetDirection * predictionTime); // Calculate the predicted target pos with the current target pos + its direction multiplied by the prediction time
			

			// And shoot when aligned
			if (CollisionRayCircle(agentInfo.Position, currentDirection, predictedTarget, 0.2f))
			{
				Shoot();
				m_ReadyToShoot = false;
			}
		}
	}

	steering = m_FaceSteering.CalculateSteering(m_pInterface->Agent_GetInfo());
}

void ItemUsage::Shoot()
{
	bool shotDone = false;
	
	// Go through each slot in the inventory
	bool noMorePistolsInInvetory = true;
	const int nrOfSlots = m_pInterface->Inventory_GetCapacity();
	for (auto currentSlot = 0; currentSlot < nrOfSlots; currentSlot++)
	{
		// If a slot has an item
		ItemInfo itemInSlot;
		if (m_pInterface->Inventory_GetItem(currentSlot, itemInSlot))
		{
			// If that item is a pistol
			if (itemInSlot.Type == eItemType::PISTOL)
			{
				if (shotDone == false)
				{
					// Try to shoot (will only work if the gun has at least 1 bullet)
					if (m_pInterface->Inventory_UseItem(currentSlot) == false)
					{
						// If it fails, throw the empty gun away
						m_pInterface->Inventory_RemoveItem(currentSlot);
					}
					else // If it works
					{
						shotDone = true;
						// If it was the last bullet in the pistol, throw it away
						if (m_pInterface->Weapon_GetAmmo(itemInSlot) == 0)
							m_pInterface->Inventory_RemoveItem(currentSlot);
						else
						{
							// If not, remember there are still pistols in the inventory and break the for loop
							noMorePistolsInInvetory = false;
							break;
						}
					}
				}
				else
				{
					// If a pistol remains in the inventory after the shot, remember it and break the for loop
					noMorePistolsInInvetory = false;
					break;
				}
			}
		}
	}

	if(noMorePistolsInInvetory)
		m_PistolAvailable = false;
}

bool ItemUsage::CollisionRayCircle(Elite::Vector2 rayOriginPoint, Elite::Vector2 rayDirection, Elite::Vector2 circleCenter, float circleRadius)
{
	// Inspired in DMGregory's answer in https://gamedev.stackexchange.com/questions/96459/fast-ray-sphere-collision-code
	
	// Calculate ray start's offset from the sphere center
	auto vecCircleCenterRayOrigin = rayOriginPoint - circleCenter;

	const auto circleRadiusSquared = circleRadius * circleRadius;
	const auto vecCircleOriginDot = vecCircleCenterRayOrigin.Dot(rayDirection);

	// The sphere is behind or surrounding the start point.
	if (vecCircleOriginDot > 0 || vecCircleCenterRayOrigin.Dot(vecCircleCenterRayOrigin) < circleRadiusSquared)
		return false;
	else
	{
		// Flatten ray origin into the plane passing through circle center perpendicular to the ray.
		// This gives the closest approach of the ray to the center.
		const auto approach = vecCircleCenterRayOrigin - vecCircleOriginDot * rayDirection;

		const auto approachSquared = approach.Dot(approach);

		// Closest approach is outside the sphere.
		if (approachSquared > circleRadiusSquared)
			return false;
		else
			return true;
	}
}




