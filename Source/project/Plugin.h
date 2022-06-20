#pragma once
#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"
#include "FiniteStateMachine.h"
#include "SteeringBehaviour.h"

class ItemUsage;
class FSMTransition;
class FSMState;
class IBaseInterface;
class IExamInterface;

struct WeightedBehavior
{
	SteeringBehaviour* behavior;
	float weight;
};

using BotFinalBehavior = vector<WeightedBehavior>;


class Plugin :public IExamPlugin
{
public:
	Plugin() {};
	virtual ~Plugin() {};

	void Initialize(IBaseInterface* pInterface, PluginInfo& info) override;
	void DllInit() override;
	void DllShutdown() override;

	void InitGameDebugParams(GameDebugParams& params) override;
	void Update(float dt) override;

	SteeringPlugin_Output UpdateSteering(float dt) override;
	void Render(float dt) const override;

private:
	//Interface, used to request data from/perform actions with the AI Framework
	IExamInterface* m_pInterface = nullptr;
	vector<HouseInfo> GetHousesInFOV() const;
	vector<EntityInfo> GetEntitiesInFOV() const;

	Elite::Vector2 m_Target = {};
	bool m_CanRun = false; //Demo purpose
	bool m_GrabItem = false; //Demo purpose
	bool m_UseItem = false; //Demo purpose
	bool m_RemoveItem = false; //Demo purpose
	float m_AngSpeed = 0.f; //Demo purpose

	// IDT
	void SetUpMovementFSM();
	FiniteStateMachine* m_MovementFSM;
	std::vector<FSMState*> m_pMovementStates{};
	std::vector<FSMTransition*> m_pMovementTransitions{};
	ItemUsage* m_ItemUsage;
	Elite::Vector2 m_SteeringDirection{};
};

//ENTRY
//This is the first function that is called by the host program
//The plugin returned by this function is also the plugin used by the host program
extern "C"
{
	__declspec (dllexport) IPluginBase* Register()
	{
		return new Plugin();
	}
}