#pragma once
#include "FiniteStateMachine.h"
#include "SteeringBehaviour.h"
#include <IExamInterface.h>
#include "Observer.h"


// STATES
// ------
class WanderLookingBackState : public FSMState
{
public:
	WanderLookingBackState() : FSMState() {}
	void OnEnter(IExamInterface* pInterface) override
	{
		// Save initial stamina and health
		const auto agentInfo = pInterface->Agent_GetInfo();
		m_InitialStamina = agentInfo.Stamina;
		m_AgentHP = agentInfo.Health;

		m_Sprinting = false;
		m_CheckBehind = false;
		m_CheckBehindTimer = 0.f;
		m_AlreadyTurnedBackwards = false;
		m_TurnBackTimer = 0.f;
		m_AlreadyTurnedForwards = false;
		m_TurnForwardTimer = 0.f;
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		auto finalSteering = m_Wander.CalculateSteering(agentInfo);

		// If dying of lack of energy, run around aimlessly in hopes of finding a house with food
		if (agentInfo.Energy <= 0.f)
			m_Sprinting = true;
		
		// If sprinting
		if (m_Sprinting)
		{
			// Activate run, if the stamina meant for the sprint has not been totally used yer
			// And if the agent's stamina isn't at 0
			if (m_InitialStamina - agentInfo.Stamina < m_SprintStamina && agentInfo.Stamina > 0.f)
			{
				finalSteering.RunMode = true;
			}
			else
			{
				finalSteering.RunMode = false;
				m_Sprinting = false;
			}
		}
		else // If walking normally
		{			
			// Look back occasionally, in case the agent's being followed

			if(m_CheckBehind) // If already looking behind
			{
				// Turn around for 1.2 seconds every 3 seconds, as to check if the agent is being tailed
				
				if(m_AlreadyTurnedBackwards) // If the direction has already been changed to the agent's back
				{
					// Start the turning back timer
					if (m_TurnBackTimer < m_TurningTime)
					{
						m_TurnBackTimer += deltaTime;
					}
					else // Once the agent's fully backwards (aka, the timer ends)
					{
						if (m_AlreadyTurnedForwards) // If the direction has already been changed to the agent's original front
						{
							// Start the turning forward timer
							if (m_TurnForwardTimer < m_TurningTime)
							{
								m_TurnForwardTimer += deltaTime;
							}
							else // And once the agent's fully forwards again (aka, the 2nd timer ends)
							{
								// Reset all the variables to go back to wandering
								m_TurnBackTimer -= m_TurningTime;
								m_TurnForwardTimer -= m_TurningTime;
								m_AlreadyTurnedBackwards = false;
								m_AlreadyTurnedForwards = false;
								m_CheckBehind = false;
							}
						}
						else  // If the direction hasn't been changed to the agent's original front yet
						{
							// Change the target once again to turn forward
							const auto invertedTarget = agentInfo.Position - finalSteering.LinearVelocity;
							m_TurnAroundSeek.SetTarget(invertedTarget);
							m_AlreadyTurnedForwards = true;
						}
					}
				}
				else // If the direction hasn't been changed to the agent's back yet
				{
					// Change target (aka, start turning to look behind)
					const auto invertedTarget = agentInfo.Position - finalSteering.LinearVelocity;
					m_TurnAroundSeek.SetTarget(invertedTarget);
					m_AlreadyTurnedBackwards = true;
				}
				
				finalSteering = m_TurnAroundSeek.CalculateSteering(agentInfo);
			}
			else  // If wandering forward normally
			{
				m_CheckBehindTimer += deltaTime;
				if(m_CheckBehindTimer >= m_CheckBehindInterval) // If the timer is done, look behind
				{
					m_CheckBehindTimer -= m_CheckBehindInterval;
					m_CheckBehind = true;
				}
				else // If the timer hasn't run out
				{
					// Check if the agent was just damaged, and if so sprint
					if (agentInfo.Health < m_AgentHP || agentInfo.Health > m_AgentHP)
					{
						if (agentInfo.Health < m_AgentHP && agentInfo.Stamina > m_MinimumToSprint)
						{
							// Reset initial stamina and sprint (or continue sprinting for longer)
							m_InitialStamina = agentInfo.Stamina;
							m_Sprinting = true;
						}
						m_AgentHP = agentInfo.Health;
					}
				}
			}
		}
		
		return finalSteering;
	}

private:
	Wander m_Wander;
	Seek m_TurnAroundSeek;
	const float m_SprintStamina{ 5.f };
	const float m_MinimumToSprint{ 2.f };
	float m_InitialStamina{};
	float m_AgentHP{};
	bool m_Sprinting{};
	
	bool m_CheckBehind{};
	float m_CheckBehindTimer{ 0.f };
	const float m_CheckBehindInterval{ 3.f };
	bool m_AlreadyTurnedBackwards{};
	float m_TurnBackTimer{};
	bool m_AlreadyTurnedForwards{};
	float m_TurnForwardTimer{};
	const float m_TurningTime{ 1.2f };
};

class SeekItemsState : public FSMState
{
public:
	SeekItemsState() : FSMState() {}
	
	void OnEnter(IExamInterface* pInterface) override
	{
		// Check if there are any items in the FOV
		vector<EntityInfo> itemsInFOV = GetItemsInFOV(pInterface);

		// If there are, change the target to the closest one
		if (!itemsInFOV.empty())
		{
			const auto agentInfo = pInterface->Agent_GetInfo();
			EntityInfo closestItem = itemsInFOV[0];

			for (const auto& item : itemsInFOV)
			{
				if (item.Location.Distance(agentInfo.Position) < closestItem.Location.Distance(agentInfo.Position))
					closestItem = item;
			}

			m_Behaviour.SetTarget(closestItem.Location);
			m_CurrentlySeeking = true;
		}
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		
		// Check if there are any items in the FOV
		vector<EntityInfo> itemsInFOV = GetItemsInFOV(pInterface);

		// If there are
		if (!itemsInFOV.empty())
		{
			PickUpCloseItems(itemsInFOV, pInterface);

			if (itemsInFOV.empty() == false) // If there are still any items left in the FOV after potentially removing the grabbed one
			{
				// Calculate the closest item that isn't being seeked
				EntityInfo closestNotSeekedItem = itemsInFOV[0];
				for (const auto& item : itemsInFOV)
				{
					if (m_Behaviour.GetTarget().Position != item.Location &&
						item.Location.Distance(agentInfo.Position) < closestNotSeekedItem.Location.Distance(agentInfo.Position))
						closestNotSeekedItem = item;
				}

				// And change the target to it IF
				if (m_CurrentlySeeking == true)
				{
					// the newly calculated item is closer than the one being currently seeked
					if (closestNotSeekedItem.Location.Distance(agentInfo.Position) < m_Behaviour.GetTarget().Position.Distance(agentInfo.Position))
					{
						m_Behaviour.SetTarget(closestNotSeekedItem.Location);
						m_CurrentlySeeking = true;
					}
				}
				else
				{
					// or the item being seeked was just grabbed
					m_Behaviour.SetTarget(closestNotSeekedItem.Location);
					m_CurrentlySeeking = true;
				}
			}
			else // If the grabbed item was the last in the FOV
			{
				return m_Behaviour.CalculateSteering(agentInfo);
			}
		}

		return m_Behaviour.CalculateSteering(agentInfo);
	}

private:
	vector<EntityInfo> GetItemsInFOV(IExamInterface* pInterface)
	{
		vector<EntityInfo> itemsInFOV = {};
		for (int i = 0;; ++i)
		{
			EntityInfo entityInfo = {};
			if (pInterface->Fov_GetEntityByIndex(i, entityInfo))
			{
				if (entityInfo.Type == eEntityType::ITEM)
					itemsInFOV.push_back(entityInfo);

				continue;
			}
			break;
		}

		return itemsInFOV;
	}

	void PickUpCloseItems(vector<EntityInfo>& itemsInFOV, IExamInterface* pInterface)
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		
		for (const auto& item : itemsInFOV)
		{
			// If any item is close enough to be grabbed
			if (agentInfo.GrabRange > agentInfo.Position.Distance(item.Location))
			{
				ItemInfo actualItem;

				// Do it
				if (pInterface->Item_Grab(item, actualItem))
				{
					// Remove the grabbed item from the itemsInFOV vector
					auto it = remove_if(itemsInFOV.begin(), itemsInFOV.end(),
						[item](const EntityInfo& entity)
						{ return entity.EntityHash == item.EntityHash; });

					itemsInFOV.erase(it, itemsInFOV.end());
					m_CurrentlySeeking = false;

					// Check if the item is worth keeping
					bool worthKeeping = true;
					switch (actualItem.Type) // Check if the item has at least 1 unit and isn't garbage
					{
					case eItemType::MEDKIT:
						if (pInterface->Medkit_GetHealth(actualItem) == 0)
							worthKeeping = false;
						break;
					case eItemType::FOOD:
						if (pInterface->Food_GetEnergy(actualItem) == 0)
							worthKeeping = false;
						break;
					case eItemType::PISTOL:
						if (pInterface->Weapon_GetAmmo(actualItem) == 0)
							worthKeeping = false;
						break;
					case eItemType::GARBAGE:
						worthKeeping = false;
						break;
					default:
						break;
					}

					// And go over each individual inventory slot
					vector<int> medKitsSlots{};
					vector<int> foodSlots{};
					vector<int> pistolsSlots{};
					const int nrOfSlots = pInterface->Inventory_GetCapacity();
					for (auto currentSlot = 0; currentSlot < nrOfSlots; currentSlot++)
					{
						ItemInfo itemAlreadyInSlot;

						// If the slot is empty, store the found item in it
						if (pInterface->Inventory_GetItem(currentSlot, itemAlreadyInSlot) == false)
						{
							pInterface->Inventory_AddItem(currentSlot, actualItem);

							// And if it's a not worth keeping item, throw it out right away
							// (this way, the agent still gets the 2 points for picking it up)
							if (worthKeeping == false)
								pInterface->Inventory_RemoveItem(currentSlot);

							break;
						}
						else // If not, try the next slot
						{
							switch(itemAlreadyInSlot.Type) // But still update the item type counters before moving on
							{
							case eItemType::MEDKIT:
								medKitsSlots.push_back(currentSlot);
								break;
							case eItemType::FOOD:
								foodSlots.push_back(currentSlot);
								break;
							case eItemType::PISTOL:
								pistolsSlots.push_back(currentSlot);
								break;
							default:
								break;
							}
							
							if (currentSlot == nrOfSlots - 1) // If there are no more slots
							{			
								if (worthKeeping) // And if the found item is worth keeping
								{
									// Clear a slot with the item type there's the most of, and store it there
									// In the case of a tie, food is favored above all, followed by medkits and then finally pistols
									int slotToReplace = 0;

									if (foodSlots.size() > medKitsSlots.size() && foodSlots.size() > pistolsSlots.size())
									{
										int leastFoodSlot = foodSlots[0]; // Find which food slot has the least amount of food
										int smallestFoodAmount = 10; // 10 is a random big number, just for the impossible scenario the first slot GetItem fails
										ItemInfo food{};
										if (pInterface->Inventory_GetItem(leastFoodSlot, food))
											smallestFoodAmount = pInterface->Food_GetEnergy(food);
										
										for(auto foodSlot : foodSlots)
										{
											if(pInterface->Inventory_GetItem(foodSlot, food))
											{
												if(smallestFoodAmount > pInterface->Food_GetEnergy(food))
												{
													leastFoodSlot = foodSlot;
													smallestFoodAmount = pInterface->Food_GetEnergy(food);
												}
											}
										}

										if (int(foodSlots.size()) > leastFoodSlot)
											slotToReplace = foodSlots[leastFoodSlot]; // And replace that one
									}
									else
									{
										if (medKitsSlots.size() >= foodSlots.size() && medKitsSlots.size() > pistolsSlots.size())
										{
											int leastMedHealthSlot = medKitsSlots[0]; // Find which medkit slot has the least amount of health
											int smallestMedHealthAmount = 10; // 10 is a random big number, just for the impossible scenario the first slot GetItem fails
											ItemInfo medkit{};
											if (pInterface->Inventory_GetItem(leastMedHealthSlot, medkit))
												smallestMedHealthAmount = pInterface->Medkit_GetHealth(medkit);

											for (auto medkitSlot : medKitsSlots)
											{
												if (pInterface->Inventory_GetItem(medkitSlot, medkit))
												{
													if (smallestMedHealthAmount > pInterface->Medkit_GetHealth(medkit))
													{
														leastMedHealthSlot = medkitSlot;
														smallestMedHealthAmount = pInterface->Medkit_GetHealth(medkit);
													}
												}
											}

											if (int(medKitsSlots.size()) > leastMedHealthSlot)
												slotToReplace = medKitsSlots[leastMedHealthSlot]; // And replace that one
										}
										else
										{
											int leastAmmoSlot = pistolsSlots[0]; // Find which pistol slot has the least amount of ammo
											int smallestAmmoAmount = 20; // 20 is a random big number, just for the impossible scenario the first slot GetItem fails
											ItemInfo pistol{};
											if (pInterface->Inventory_GetItem(leastAmmoSlot, pistol))
												smallestAmmoAmount = pInterface->Weapon_GetAmmo(pistol);

											for (auto pistolSlot : pistolsSlots)
											{
												if (pInterface->Inventory_GetItem(pistolSlot, pistol))
												{
													if (smallestAmmoAmount > pInterface->Weapon_GetAmmo(pistol))
													{
														leastAmmoSlot = pistolSlot;
														smallestAmmoAmount = pInterface->Weapon_GetAmmo(pistol);
													}
												}
											}

											if(int(pistolsSlots.size()) > leastAmmoSlot)
												slotToReplace = pistolsSlots[leastAmmoSlot]; // And replace that one
										}
									}

									// Finally, replace the item in the chosen slot for the new item
									pInterface->Inventory_RemoveItem(slotToReplace);
									pInterface->Inventory_AddItem(slotToReplace, actualItem);
									break;
								}
								else // If it's not worth keeping, destroy it without picking it up
								{
									pInterface->Item_Destroy(item);
								}
							}
						}
					}

					// And notify the ItemUsage Observer if the item was stored
					if (worthKeeping)
					{
						switch (actualItem.Type)
						{
						case eItemType::MEDKIT:
							m_Subject->Notify(Event::MedkitPickedUp);
							break;
						case eItemType::FOOD:
							m_Subject->Notify(Event::FoodPickedUp);
							break;
						case eItemType::PISTOL:
							m_Subject->Notify(Event::PistolPickedUp);
							break;
						default:
							break;
						}
					}
				}
			}
		}
	}
	
	Seek m_Behaviour;
	bool m_CurrentlySeeking{ false };
};

class FleeEnemiesState : public FSMState
{
public:
	FleeEnemiesState() : FSMState() {}

	void OnEnter(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		
		// Save initial stamina and health
		m_InitialStamina = agentInfo.Stamina;
		m_AgentHP = agentInfo.Health;

		// Start sprinting
		m_Sprinting = true;		
	}

	void OnExit(IExamInterface* pInterface) override
	{
		m_EnemiesNearby.clear();
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();

		// Check if the agent was just damaged, and if so sprint
		if (agentInfo.Health < m_AgentHP || agentInfo.Health > m_AgentHP)
		{
			if (agentInfo.Health < m_AgentHP && agentInfo.Stamina > m_MinimumToSprint)
			{
				// Reset initial stamina and sprint (or continue sprinting for longer)
				m_InitialStamina = agentInfo.Stamina;
				m_Sprinting = true;
			}
			m_AgentHP = agentInfo.Health;
		}
		
		// Check if there are any new enemies nearby and add them to the vector
		for (int i = 0;; ++i)
		{
			EntityInfo spottedEntity = {};
			if (pInterface->Fov_GetEntityByIndex(i, spottedEntity))
			{
				if (spottedEntity.Type == eEntityType::ENEMY)
				{
					bool alreadyStored{ false };
					
					for (const auto& knownEnemy : m_EnemiesNearby)
					{
						if (spottedEntity.EntityHash == knownEnemy.EntityHash)
						{
							alreadyStored = true;
							break;
						}
					}

					if (alreadyStored == false)
					{
						m_EnemiesNearby.push_back(spottedEntity);
						std::cout << "New enemy spotted! Size of vector now: " << m_EnemiesNearby.size() << '\n';

						if (agentInfo.Stamina > m_MinimumToSprint)
						{
							// Reset initial stamina and sprint (or continue sprinting for longer)
							m_InitialStamina = agentInfo.Stamina;
							m_Sprinting = true;
						}
					}
				}

				continue;
			}
			break;
		}

		// Remove far away enemies from the vector
		auto removed = remove_if(m_EnemiesNearby.begin(), m_EnemiesNearby.end(), [&](const EntityInfo& enemy)
			{
				if (enemy.Location.Distance(agentInfo.Position) > m_FleeDistance)
				{
					std::cout << "Escaped from enemy! Size of vector now: " << m_EnemiesNearby.size() - 1 << '\n';
					return true;
				}
				else
					return false;
			});

		m_EnemiesNearby.erase(removed, m_EnemiesNearby.end());
		
		
		// Create all the needed flee behaviors with their respective weight (one for each spotted enemy)
		if (m_EnemiesNearby.empty() == false)
		{
			std::vector<std::pair<Flee, float>> weightedFlees{};

			// Create and store each flee
			float combinedEnemyDistances{};
			for (const auto& enemy : m_EnemiesNearby)
			{
				Flee enemyFlee;
				enemyFlee.SetTarget(enemy.Location);
				const auto distance = enemy.Location.Distance(agentInfo.Position);
				combinedEnemyDistances += distance;
				auto pair = std::pair<Flee, float>(enemyFlee, distance); // The paired float is still only the distance, it will be recalculated into the unscaled weight later
				weightedFlees.push_back(pair);
			}

			// Calculate each weight from closest to furthest
			for (auto weightedFlee : weightedFlees)
				weightedFlee.second = combinedEnemyDistances - weightedFlee.second;

			// Execute the blended steering from all the stored behaviors
			if (weightedFlees.empty() == false)
			{
				if (weightedFlees.size() == 1)
				{
					// Activate run only for a short sprint
					auto singleFleeSteering = weightedFlees[0].first.CalculateSteering(agentInfo);
					if (m_Sprinting)
					{
						if (m_InitialStamina - agentInfo.Stamina < m_SprintStamina && agentInfo.Stamina > 0.f)
						{
							singleFleeSteering.RunMode = true;
						}
						else
						{
							singleFleeSteering.RunMode = false;
							m_Sprinting = false;
						}
					}
					return singleFleeSteering;
				}
				else
				{
					SteeringPlugin_Output finalSteering{};

					float combinedWeight{};

					for (auto weightedFlee : weightedFlees)
					{
						// Calculate the combined weight of all flees
						combinedWeight += weightedFlee.second;

						// Calculate the blended steering from all the flees, according to their respective weight
						auto steering = weightedFlee.first.CalculateSteering(agentInfo);
						finalSteering.LinearVelocity += weightedFlee.second * steering.LinearVelocity;
						finalSteering.AngularVelocity += weightedFlee.second * steering.AngularVelocity;
					}

					// Scale the steering output according to the scale
					if (combinedWeight > 0.f)
					{
						const auto scale = 1.f / combinedWeight;
						finalSteering.LinearVelocity *= scale;
						finalSteering.AngularVelocity *= scale;
					}

					// Activate run only for a short sprint
					if (m_Sprinting)
					{
						if (m_InitialStamina - agentInfo.Stamina < m_SprintStamina && agentInfo.Stamina > 0.f)
						{
							finalSteering.RunMode = true;
						}
						else
						{
							finalSteering.RunMode = false;
							m_Sprinting = false;
						}
					}
					return finalSteering;
				}
			}
			else
			{
				return SteeringPlugin_Output{};
			}
		}
	}

private:
	vector<EntityInfo> m_EnemiesNearby;
	const float m_FleeDistance{ 50.f };
	const float m_SprintStamina{ 5.f };
	const float m_MinimumToSprint{ 2.f };
	bool m_Sprinting{};
	float m_InitialStamina{};
	float m_AgentHP{};
};

class SeekHouseState : public FSMState
{
public:
	SeekHouseState() : FSMState() {}

	void OnEnter(IExamInterface* pInterface) override
	{
		m_SeekedHouse = HouseInfo{};
		m_InitialStamina = 0.f;
		m_CurrentlyRunning = false;
		m_RecoveringStamina = false;
		m_NotMovingCounter = 0.f;
		m_Stuck = false;
		m_TryToUnstuckCounter = false;
		
		// Check if there are any houses inside the FOV
		vector<HouseInfo> spottedHouses = {};
		HouseInfo spottedHouse = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetHouseByIndex(i, spottedHouse))
			{
				spottedHouses.push_back(spottedHouse);
				continue;
			}
			break;
		}
		
		// If there are, change the target the closest one
		if (!spottedHouses.empty())
		{
			const auto agentInfo = pInterface->Agent_GetInfo();
			m_SeekedHouse = spottedHouses[0];

			for (const auto& house : spottedHouses)
			{
				if (house.Center.Distance(agentInfo.Position) < m_SeekedHouse.Center.Distance(agentInfo.Position))
					m_SeekedHouse = house;
			}
		}
	}

	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();

		if (m_Stuck)
		{
			// If stuck, make the agent wander for 3 seconds
			m_TryToUnstuckCounter += deltaTime;
			if (m_TryToUnstuckCounter > m_TimeToUnstuck)
			{
				m_TryToUnstuckCounter = 0.f;
				m_Stuck = false; // Consider them unstuck
			}
			return m_WanderToUnstuck.CalculateSteering(agentInfo);
		}
		else
		{
			if (agentInfo.CurrentLinearSpeed < 0.5f) // If the agent isn't moving much
			{
				m_NotMovingCounter += deltaTime;
				if (m_NotMovingCounter > m_TimeStoppedToConsiderStuck) // If they keep not moving for 0.5 seconds
				{
					m_NotMovingCounter = 0.f;
					m_Stuck = true; // Consider them stuck
				}
			}

			// Calculate seek to the house center
			m_SeekHouseCenter.SetTarget(pInterface->NavMesh_GetClosestPathPoint(m_SeekedHouse.Center));
			auto finalSteering = m_SeekHouseCenter.CalculateSteering(agentInfo);

			// Run in sprints
			if (m_CurrentlyRunning) // If already running
			{
				if (m_InitialStamina - agentInfo.Stamina > m_SprintStamina || agentInfo.Stamina <= 0.f) // If the sprint has been complete
				{
					// Stop running
					finalSteering.RunMode = false;
					m_CurrentlyRunning = false;
					m_RecoveringStamina = true;
				}
				else // If still sprinting
				{
					// Just continue running
					finalSteering.RunMode = true;
				}
			}
			else // Else, if not running already
			{
				if (m_RecoveringStamina == false) // If it's the first time updating (aka, not recovering stamina but not running yet)
				{
					if (agentInfo.Stamina >= m_SprintStamina)
					{
						// Save the initial stamina value and start running
						m_InitialStamina = agentInfo.Stamina;
						finalSteering.RunMode = true;
						m_CurrentlyRunning = true;
					}
				}
				else
				{
					if (m_InitialStamina <= agentInfo.Stamina) // If the stamina wasted in the sprint is replenished
					{
						// Start sprinting again
						finalSteering.RunMode = true;
						m_CurrentlyRunning = true;
						m_RecoveringStamina = false;
					}
					else // If still recovering stamina
					{
						// Don't run
						finalSteering.RunMode = false;
					}
				}
			}

			return finalSteering;
		}
	}

private:
	Seek m_SeekHouseCenter;
	Wander m_WanderToUnstuck;
	HouseInfo m_SeekedHouse;
	const float m_SprintStamina{ 3.f };
	float m_InitialStamina{};
	bool m_CurrentlyRunning{ false };
	bool m_RecoveringStamina{ false };
	float m_NotMovingCounter{};
	const float m_TimeStoppedToConsiderStuck{ 0.5f };
	bool m_Stuck{};
	float m_TryToUnstuckCounter{};
	const float m_TimeToUnstuck{ 3.f };
};

class LookAroundHouseState : public FSMState
{
public:
	LookAroundHouseState() : FSMState() {}

	void OnEnter(IExamInterface* pInterface) override
	{
		// If the house is recognizable in the FOV, store it
		HouseInfo spottedHouse = {};
		if (pInterface->Fov_GetHouseByIndex(0, spottedHouse))
		{
			m_House = spottedHouse;
			m_HouseFound = true;
			m_Seek.SetTarget(spottedHouse.Center);
		}
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		if (m_HouseFound)
		{
			// If still inside the house, wander
			if (pInterface->Agent_GetInfo().IsInHouse)
			{
				auto wander = m_Wander.CalculateSteering(pInterface->Agent_GetInfo());
				wander.AutoOrient = false;
				wander.AngularVelocity = pInterface->Agent_GetInfo().MaxAngularSpeed; // Spin around looking for items
				return wander;
				//return m_Wander.CalculateSteering(pInterface->Agent_GetInfo());
			}
			else // But if the agent ever accidentally leaves the house, seek the house center just enough to re-enter it
				return m_Seek.CalculateSteering(pInterface->Agent_GetInfo());
		}
		else
		{
			// Wander normally, but looking to recognize the house (aka, look at a wall)
			OnEnter(pInterface);
			return m_Wander.CalculateSteering(pInterface->Agent_GetInfo());
		}
	}

private:
	Wander m_Wander;
	Seek m_Seek;
	HouseInfo m_House;
	bool m_HouseFound;
};

class ExitHouseState : public FSMState
{
public:
	ExitHouseState() : FSMState() {}

	void OnEnter(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		m_OutsidePosSet = false;
		m_NotMovingCounter = 0.f;
		m_Stuck = false;
		m_TryToUnstuckCounter = 0.f;
		
		// Check if the agent can spot the house they're on in the FOV
		vector<HouseInfo> spottedHouses = {};
		HouseInfo spottedHouse = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetHouseByIndex(i, spottedHouse))
			{
				spottedHouses.push_back(spottedHouse);
				continue;
			}
			break;
		}

		// If so, set the target to somewhere outside the premises
		if (spottedHouses.empty() == false)
		{
			m_PositionOutsideHouse = spottedHouses[0].Center + spottedHouses[0].Size;
			m_OutsidePosSet = true;
		}
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();

		if(m_OutsidePosSet) // If the target was successfully set in the OnEnter
		{
			if(m_Stuck)
			{
				// If stuck, make the agent wander for 3 seconds
				m_TryToUnstuckCounter += deltaTime;
				if (m_TryToUnstuckCounter > m_TimeToUnstuck)
				{
					m_TryToUnstuckCounter = 0.f;
					m_Stuck = false; // Consider them unstuck
				}
				return m_WanderAround.CalculateSteering(agentInfo);
			}
			else
			{
				if (agentInfo.CurrentLinearSpeed < 0.5f) // If the agent isn't moving much
				{
					m_NotMovingCounter += deltaTime;
					if (m_NotMovingCounter > m_TimeStoppedToConsiderStuck) // If they keep not moving for 0.5 seconds
					{
						m_NotMovingCounter = 0.f;
						m_Stuck = true; // Consider them stuck
					}
				}
				
				m_SeekOutsideHouse.SetTarget(pInterface->NavMesh_GetClosestPathPoint(m_PositionOutsideHouse));
				return m_SeekOutsideHouse.CalculateSteering(agentInfo);
			}
		}
		else // If not, keep looking for the house
		{
			// Redo all the OnEnter code
			OnEnter(pInterface);

			// And wander around until the target is set
			return m_WanderAround.CalculateSteering(agentInfo);
			
		}
	}

private:
	Seek m_SeekOutsideHouse;
	Wander m_WanderAround;
	Elite::Vector2 m_PositionOutsideHouse;
	bool m_OutsidePosSet{};
	float m_NotMovingCounter{};
	const float m_TimeStoppedToConsiderStuck{ 0.5f };
	bool m_Stuck{};
	float m_TryToUnstuckCounter{};
	const float m_TimeToUnstuck{ 3.f };
};

class ComeBackToTownState : public FSMState
{
public:
	ComeBackToTownState() : FSMState() {}

	void OnEnter(IExamInterface* pInterface) override
	{
		m_SeekTownCenter.SetTarget(pInterface->World_GetInfo().Center);
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		return m_SeekTownCenter.CalculateSteering(pInterface->Agent_GetInfo());
	}

private:
	Seek m_SeekTownCenter;
};

class FleePurgeZonesState : public FSMState
{
public:
	FleePurgeZonesState() : FSMState() {}

	void OnEnter(IExamInterface* pInterface) override
	{
		// Initialize the ExitHouseBehaviour
		m_ExitHouseBehaviour.OnEnter(pInterface);
	}
	
	SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) override
	{
		auto agentInfo = pInterface->Agent_GetInfo();

		// If inside a house, exit it first
		if (agentInfo.IsInHouse)
			return m_ExitHouseBehaviour.Update(deltaTime, pInterface);
		
		// Save all purge zones in the FOV
		vector<EntityInfo> purgeZonesInFOV;
		EntityInfo entity = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetEntityByIndex(i, entity))
			{
				if (entity.Type == eEntityType::PURGEZONE)
					purgeZonesInFOV.push_back(entity);

				continue;
			}

			break;
		}

		// If any new purge zones were spotted
		if (purgeZonesInFOV.empty() == false)
		{
			PurgeZoneInfo zoneInfo;

			// If no purge zone is being fled from already, make the first in the FOV the target
			if (m_PurgeZoneCenter == Elite::Vector2{})
			{
				pInterface->PurgeZone_GetInfo(purgeZonesInFOV[0], zoneInfo);
				m_PurgeZoneCenter = zoneInfo.Center;
				m_PurgeZoneRadius = zoneInfo.Radius;
				m_FleeBehavior.SetTarget(m_PurgeZoneCenter);
			}

			// Then go over all the spotted purge zones
			for (const auto& purgeZone : purgeZonesInFOV)
			{
				pInterface->PurgeZone_GetInfo(purgeZone, zoneInfo);

				// And if any one of them is closer then the one being fled from, change the target
				if (zoneInfo.Center.Distance(agentInfo.Position) - zoneInfo.Radius < m_PurgeZoneCenter.Distance(agentInfo.Position) - m_PurgeZoneRadius)
				{
					m_PurgeZoneCenter = zoneInfo.Center;
					m_PurgeZoneRadius = zoneInfo.Radius;
					m_FleeBehavior.SetTarget(m_PurgeZoneCenter);
				}
			}
		}

		return m_FleeBehavior.CalculateSteering(agentInfo);
	}

private:
	Flee m_FleeBehavior;
	ExitHouseState m_ExitHouseBehaviour;
	Elite::Vector2 m_PurgeZoneCenter{};
	float m_PurgeZoneRadius{};
};




// TRANSITIONS
// -----------

class EnemySpotted : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}
	
	bool ToTransition(IExamInterface* pInterface) override
	{
		// Check if there's enemies nearby
		vector<EntityInfo> enemiesNearby = {};
		for (int i = 0;; ++i)
		{
			EntityInfo ei = {};
			if (pInterface->Fov_GetEntityByIndex(i, ei))
			{
				if (ei.Type == eEntityType::ENEMY)
					enemiesNearby.push_back(ei);

				continue;
			}
			break;
		}

		// If there are, return true
		if (enemiesNearby.empty() == false)
			return true;
		else
			return false;
	}
};

class EscapedFromEnemies : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
		m_TransitionTimer += deltaTime;
	}
	
	bool ToTransition(IExamInterface* pInterface) override
	{
		// Check if there's enemies nearby
		vector<EntityInfo> enemiesNearby = {};
		for (int i = 0;; ++i)
		{
			EntityInfo ei = {};
			if (pInterface->Fov_GetEntityByIndex(i, ei))
			{
				if (ei.Type == eEntityType::ENEMY)
					enemiesNearby.push_back(ei);

				continue;
			}
			break;
		}

		// If there are, re-start the transition timer
		if (enemiesNearby.empty() == false)
			m_TransitionTimer = 0.f;

		// If the timer runs out, return true
		if(m_TransitionTimer > m_TransitionMaxTime)
		{
			m_TransitionTimer = 0.f;
			return true;
		}
		
		return false;

		// NOTE: The timer delays the transitions enough for the agent to sprint away
		// While still looking for more enemies in front of them (which will restart the timer)
	}
private:
	float m_TransitionTimer{};
	const float m_TransitionMaxTime{ 4.f };
};


class NewHouseSpotted : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
		// Forget any previously ransacked house after 90 seconds
		// As new items might've already spawned in it in the meantime
		for (auto i = int(m_RansackedHouses.size()) - 1; i >= 0; i--)
		{
			if (m_RansackedHouses[i].second + m_ResetHousesInterval < pInterface->World_GetStats().TimeSurvived)
				m_RansackedHouses.erase(m_RansackedHouses.begin() + i);
		}
	}
	
	bool ToTransition(IExamInterface* pInterface) override
	{
		// Check if there are any not ransacked houses inside the FOV
		vector<HouseInfo> spottedHouses = {};
		HouseInfo spottedHouse = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetHouseByIndex(i, spottedHouse))
			{
				bool isNew = true;
				for (const auto& ransackedHouse : m_RansackedHouses)
				{
					if (spottedHouse.Center == ransackedHouse.first.Center)
					{
						isNew = false;
						break;
					}
				}

				if(isNew)
					spottedHouses.push_back(spottedHouse);
				
				continue;
			}
			break;
		}

		if (spottedHouses.empty()) // If no house is found, return false
		{
			return false;
		}
		else // Else, save the closest one (the one that's gonna be seeked) and return true
		{
			const auto agentInfo = pInterface->Agent_GetInfo();
			auto closestHouse = spottedHouses[0];

			for (const auto& house : spottedHouses)
			{
				if (house.Center.Distance(agentInfo.Position) < closestHouse.Center.Distance(agentInfo.Position))
					closestHouse = house;
			}

			// The first part of the stored pair is the HouseInfo, the second is the time in which it was found
			const auto currentTime = pInterface->World_GetStats().TimeSurvived;
			const auto newHouse = std::make_pair(closestHouse, currentTime);
			m_RansackedHouses.push_back(newHouse);
			return true;
		}
	}
private:
	vector<std::pair<HouseInfo, float>> m_RansackedHouses;
	const float m_ResetHousesInterval{ 90.f };
};

class HouseCenterReached : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		
		// Save all the houses in the FOV
		vector<HouseInfo> spottedHouses = {};
		HouseInfo spottedHouse = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetHouseByIndex(i, spottedHouse))
			{
				spottedHouses.push_back(spottedHouse);
				continue;
			}
			break;
		}

		// If any house is seen, store the closest (and if the target's already set, check if the new house is closer)
		if (!spottedHouses.empty())
		{
			if (m_SeekedHouse.Center == Elite::Vector2{}) // If the seeked house hasn't been stored yet
				m_SeekedHouse = spottedHouses[0]; // Save the first house in the FOV

			for (const auto& house : spottedHouses)
			{
				if (house.Center.Distance(agentInfo.Position) < m_SeekedHouse.Center.Distance(agentInfo.Position))
					m_SeekedHouse = house; // If any house is found close then the one being seeked, change the target
			}
		}

		// If a target has been set
		if (m_SeekedHouse.Center != Elite::Vector2{})
		{
			// Return if the agent's close enough to the center
			if (m_SeekedHouse.Center.Distance(agentInfo.Position) <= m_ArrivalRange)
				return true;
			else
				return false;
		}

		return false;

		
	}

private:
	HouseInfo m_SeekedHouse;
	bool m_SeekedHouseInfoStored{ false };
	float m_ArrivalRange{ 10.f };
};


class ItemSpotted : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		// Check if there's items nearby
		vector<EntityInfo> itemsNearby = {};
		for (int i = 0;; ++i)
		{
			EntityInfo ei = {};
			if (pInterface->Fov_GetEntityByIndex(i, ei))
			{
				if (ei.Type == eEntityType::ITEM)
					itemsNearby.push_back(ei);

				continue;
			}
			break;
		}

		// If there are, return true
		if (itemsNearby.empty() == false)
			return true;
		else
			return false;
	}
};

class AllItemsCloseByTaken : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
		m_TransitionTimer += deltaTime;
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		// Check if there are any items in the FOV
		vector<EntityInfo> itemsNearby = {};
		for (int i = 0;; ++i)
		{
			EntityInfo ei = {};
			if (pInterface->Fov_GetEntityByIndex(i, ei))
			{
				if (ei.Type == eEntityType::ITEM)
					itemsNearby.push_back(ei);

				continue;
			}
			break;
		}

		// If there are, re-start the transition timer
		if (itemsNearby.empty() == false)
			m_TransitionTimer = 0.f;

		// If the timer runs out, return true
		if (m_TransitionTimer > m_TransitionMaxTime)
		{
			m_TransitionTimer = 0.f;
			return true;
		}

		return false;

		// NOTE: While seeking items, the timer delay doesn't affect the agent's performance
		// And while looking around the house, it allows them to do a full spin looking around for items
		// This way, only if there are no items all around them will they change behaviour
	}
private:
	float m_TransitionTimer{};
	const float m_TransitionMaxTime{ 4.f };
};


class ExitedHouse : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();

		if (agentInfo.IsInHouse)
			return false;
		else
			return true;
	}
};

class InsideHouse : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		return agentInfo.IsInHouse;
	}
};


class ReturnedToTown : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentPos = pInterface->Agent_GetInfo().Position;
		const auto town = pInterface->World_GetInfo();

		return agentPos.x > town.Center.x - town.Dimensions.x / 2.f &&
			agentPos.x < town.Center.x + town.Dimensions.x / 2.f &&
			agentPos.y > town.Center.y - town.Dimensions.y / 2.f &&
			agentPos.y < town.Center.y + town.Dimensions.y / 2.f;
	}
};

class TooFarAwayFromTown : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}

	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentPos = pInterface->Agent_GetInfo().Position;
		const auto town = pInterface->World_GetInfo();
		const auto explorationMargin = 10.f;

		return !(agentPos.x > town.Center.x - (town.Dimensions.x / 2.f + explorationMargin) &&
			agentPos.x < town.Center.x + (town.Dimensions.x / 2.f + explorationMargin) &&
			agentPos.y > town.Center.y - (town.Dimensions.y / 2.f + explorationMargin) &&
			agentPos.y < town.Center.y + (town.Dimensions.y / 2.f + explorationMargin));
	}
};


class InsidePurgeZone : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
	}
	
	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();
		
		// Save all purge zones in the FOV
		vector<EntityInfo> purgeZonesInFOV;
		EntityInfo entity = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetEntityByIndex(i, entity))
			{
				if (entity.Type == eEntityType::PURGEZONE)
					purgeZonesInFOV.push_back(entity);

				continue;
			}
			break;
		}

		// If no purge zones were spotted, return false
		if (purgeZonesInFOV.empty())
			return false;

		// If there were, check if the agent's inside any
		for (const auto& zone : purgeZonesInFOV)
		{
			PurgeZoneInfo zoneInfo;
			pInterface->PurgeZone_GetInfo(purgeZonesInFOV[0], zoneInfo);
			
			if (zoneInfo.Center.Distance(agentInfo.Position) <= zoneInfo.Radius) // And return true if so
				return true;
		}

		return false;
	}
};

class PurgeZoneFled : public FSMTransition
{
public:
	void Update(float deltaTime, IExamInterface* pInterface) override
	{
		if (m_TimerOn)
			m_Counter += deltaTime;
	}
	
	bool ToTransition(IExamInterface* pInterface) override
	{
		const auto agentInfo = pInterface->Agent_GetInfo();

		// Save all purge zones in the FOV
		vector<EntityInfo> purgeZonesInFOV;
		EntityInfo entity = {};
		for (int i = 0;; ++i)
		{
			if (pInterface->Fov_GetEntityByIndex(i, entity))
			{
				if (entity.Type == eEntityType::PURGEZONE)
					purgeZonesInFOV.push_back(entity);

				continue;
			}
			break;
		}

		// If no purge zones were spotted
		if (purgeZonesInFOV.empty())
		{
			m_TimerOn = true; // Start or continue timer (to flee for 3 extra second)

			if(m_Counter > m_ExtraFleeTime) // And, if the timer reaches its end, return true
			{
				m_Counter = 0.f;
				m_TimerOn = false;
				return true;
			}
		}
		else // If they were
		{
			m_Counter = 0.f;
			m_TimerOn = false; // Restart timer
		}

		return false;

	}
private:
	const float m_ExtraFleeTime{ 3.f };
	float m_Counter{ 0.f };
	bool m_TimerOn{ false };
	
};

