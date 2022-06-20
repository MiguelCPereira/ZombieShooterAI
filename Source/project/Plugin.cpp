#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"
#include "StatesTransitions.h"
#include "ItemUsage.h"

//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "Botinator 2000";
	info.Student_FirstName = "Miguel";
	info.Student_LastName = "Pereira";
	info.Student_Class = "2DAE02";


	m_ItemUsage = new ItemUsage(m_pInterface);
	SetUpMovementFSM();
}

//Called only once
void Plugin::DllInit()
{
	//Called when the plugin is loaded

}

//Called only once
void Plugin::DllShutdown()
{
	//Called when the plugin gets unloaded
	
	for (auto& t : m_pMovementStates)
		SAFE_DELETE(t);
	
	for (auto& t : m_pMovementTransitions)
		SAFE_DELETE(t);

	SAFE_DELETE(m_MovementFSM);
	SAFE_DELETE(m_ItemUsage);
}

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = true; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be usefull to inspect certain behaviours (Default = false)
	params.AutoGrabClosestItem = true; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::Update(float dt)
{
	const auto& agentInfo = m_pInterface->Agent_GetInfo();
	const auto& entitiesNearby = GetEntitiesInFOV();
}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	auto finalSteering = SteeringPlugin_Output{};
	finalSteering.AutoOrient = false;
	bool currentlyAiming;
	m_ItemUsage->Update(dt, finalSteering, currentlyAiming); // Use any items required for the situation (and change steering to shoot, if needed)
	if (currentlyAiming == false)
	{
		finalSteering.AutoOrient = true;
		finalSteering = m_MovementFSM->Update(dt); // Calculate the steering through the FSM
	}
	
	m_SteeringDirection = finalSteering.LinearVelocity; // For debug drawing purposes
	

	return finalSteering;
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	//This Render function should only contain calls to Interface->Draw_... functions
	m_pInterface->Draw_Direction(m_pInterface->Agent_GetInfo().Position, m_SteeringDirection, 5.f, { 0, 1, 0 });
	m_pInterface->Draw_Circle(m_pInterface->Agent_GetInfo().Position, 12.f, { 0, 1, 1 });
}

vector<HouseInfo> Plugin::GetHousesInFOV() const
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}

vector<EntityInfo> Plugin::GetEntitiesInFOV() const
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}

void Plugin::SetUpMovementFSM()
{
	// Create all the needed states
	auto* pWanderLookingBackState = new WanderLookingBackState();
	m_pMovementStates.push_back(pWanderLookingBackState);
	auto* pFleeEnemiesState = new FleeEnemiesState();
	m_pMovementStates.push_back(pFleeEnemiesState);
	auto* pSeekHouseState = new SeekHouseState();
	m_pMovementStates.push_back(pSeekHouseState);
	auto* pLookAroundHouseState = new LookAroundHouseState();
	m_pMovementStates.push_back(pLookAroundHouseState);
	auto* pSeekItemsState = new SeekItemsState();
	pSeekItemsState->GetSubject()->AddObserver(m_ItemUsage);
	m_pMovementStates.push_back(pSeekItemsState);
	auto* pExitHouseState = new ExitHouseState();
	m_pMovementStates.push_back(pExitHouseState);
	auto* pComeBackToTownState = new ComeBackToTownState();
	m_pMovementStates.push_back(pComeBackToTownState);
	auto* pFleePurgeZonesState = new FleePurgeZonesState();
	m_pMovementStates.push_back(pFleePurgeZonesState);
	

	// Initialize the FSM
	m_MovementFSM = new FiniteStateMachine{ pWanderLookingBackState, m_pInterface };

	// Create transition to flee from enemies
	auto* pEnemySpotted = new EnemySpotted();
	m_pMovementTransitions.push_back(pEnemySpotted);
	m_MovementFSM->AddTransition(pWanderLookingBackState, pFleeEnemiesState, pEnemySpotted);

	// Create transitions to seek un-scavenged houses
	auto* pNewHouseSpotted = new NewHouseSpotted();
	m_pMovementTransitions.push_back(pNewHouseSpotted);
	m_MovementFSM->AddTransition(pFleeEnemiesState, pSeekHouseState, pNewHouseSpotted);
	m_MovementFSM->AddTransition(pWanderLookingBackState, pSeekHouseState, pNewHouseSpotted);

	// Create transitions to evacuate from a house
	auto* pAllItemsCloseByTaken = new AllItemsCloseByTaken();
	m_pMovementTransitions.push_back(pAllItemsCloseByTaken);
	m_MovementFSM->AddTransition(pLookAroundHouseState, pExitHouseState, pAllItemsCloseByTaken); // After looting said house (the most common one)
	auto* pInsideAlreadyLootedHouse = new InsideHouse();
	m_pMovementTransitions.push_back(pInsideAlreadyLootedHouse);
	m_MovementFSM->AddTransition(pWanderLookingBackState, pExitHouseState, pInsideAlreadyLootedHouse); // If the agent randomly wanders into an already looted house (which should be rare)

	// Create transitions to look around the house
	auto* pHouseCenterReached = new HouseCenterReached();
	m_pMovementTransitions.push_back(pHouseCenterReached);
	m_MovementFSM->AddTransition(pSeekHouseState, pLookAroundHouseState, pHouseCenterReached); // After arriving at the house center
	m_MovementFSM->AddTransition(pSeekItemsState, pLookAroundHouseState, pAllItemsCloseByTaken); // If all nearby items have been taken

	// Create transitions to seek items inside the house
	auto* pItemSpotted = new ItemSpotted();
	m_pMovementTransitions.push_back(pItemSpotted);
	m_MovementFSM->AddTransition(pLookAroundHouseState, pSeekItemsState, pItemSpotted);
	m_MovementFSM->AddTransition(pSeekHouseState, pSeekItemsState, pItemSpotted);
	m_MovementFSM->AddTransition(pExitHouseState, pSeekItemsState, pItemSpotted);

	// Create transitions to come back into the city (in case the agent ends up too far away from all the houses)
	auto* pTooFarAwayFromTown = new TooFarAwayFromTown();
	m_pMovementTransitions.push_back(pTooFarAwayFromTown);
	m_MovementFSM->AddTransition(pWanderLookingBackState, pComeBackToTownState, pTooFarAwayFromTown);
	m_MovementFSM->AddTransition(pFleeEnemiesState, pComeBackToTownState, pTooFarAwayFromTown);

	// Create transitions to flee from purge zones
	auto* pInsidePurgeZone = new InsidePurgeZone();
	m_pMovementTransitions.push_back(pInsidePurgeZone);
	m_MovementFSM->AddTransition(pWanderLookingBackState, pFleePurgeZonesState, pInsidePurgeZone);
	m_MovementFSM->AddTransition(pFleeEnemiesState, pFleePurgeZonesState, pInsidePurgeZone);
	m_MovementFSM->AddTransition(pSeekHouseState, pFleePurgeZonesState, pInsidePurgeZone);
	m_MovementFSM->AddTransition(pLookAroundHouseState, pFleePurgeZonesState, pInsidePurgeZone);
	m_MovementFSM->AddTransition(pSeekItemsState, pFleePurgeZonesState, pInsidePurgeZone);
	m_MovementFSM->AddTransition(pExitHouseState, pFleePurgeZonesState, pInsidePurgeZone);
	m_MovementFSM->AddTransition(pComeBackToTownState, pFleePurgeZonesState, pInsidePurgeZone);	

	// Create transition to wander
	auto* pEscapedFromEnemies = new EscapedFromEnemies();
	m_pMovementTransitions.push_back(pEscapedFromEnemies);
	m_MovementFSM->AddTransition(pFleeEnemiesState, pWanderLookingBackState, pEscapedFromEnemies); // Wander after fleeing from enemies (if they're far away enough)
	auto* pExitedHouse = new ExitedHouse();
	m_pMovementTransitions.push_back(pExitedHouse);
	m_MovementFSM->AddTransition(pExitHouseState, pWanderLookingBackState, pExitedHouse); // Wander after exiting a house
	auto* pReturnedToTown = new ReturnedToTown();
	m_pMovementTransitions.push_back(pReturnedToTown);
	m_MovementFSM->AddTransition(pComeBackToTownState, pWanderLookingBackState, pReturnedToTown); // Wander after returning to the relevant part of the map
	auto* pPurgeZoneFled = new PurgeZoneFled();
	m_pMovementTransitions.push_back(pPurgeZoneFled);
	m_MovementFSM->AddTransition(pFleePurgeZonesState, pWanderLookingBackState, pPurgeZoneFled); // Wander after fleeing from a purge zone
}
