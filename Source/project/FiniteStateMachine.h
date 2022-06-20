#pragma once
#include <Exam_HelperStructs.h>
#include <map>

#include "Subject.h"

/*=============================================================================*/
// Heavily inspired in the implementation from class
//		Copyright 2020-2021 Elite Engine
//		Author: Andries Geens
//		http://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter12_A_Reusable_Light-Weight_Finite-State_Machine.pdf
/*=============================================================================*/


class IExamInterface;

class FSMState
{
public:
	FSMState() {}
	virtual ~FSMState() = default;

	virtual void OnEnter(IExamInterface* pInterface) {}
	virtual void OnExit(IExamInterface* pInterface) {}
	virtual SteeringPlugin_Output Update(float deltaTime, IExamInterface* pInterface) { return SteeringPlugin_Output{}; }
	Subject* GetSubject() const { return m_Subject;  }

protected:
	Subject* m_Subject{ new Subject() };
};

class FSMTransition
{
public:
	FSMTransition() = default;
	virtual ~FSMTransition() = default;
	virtual void Update(float deltaTime, IExamInterface* pInterface) = 0;
	virtual bool ToTransition(IExamInterface* pInterface) = 0;
};


class FiniteStateMachine
{
public:
	FiniteStateMachine(FSMState* startState, IExamInterface* pInterface);
	~FiniteStateMachine() = default;

	void AddTransition(FSMState* startState, FSMState* toState, FSMTransition* transition);
	SteeringPlugin_Output Update(float deltaTime);

private:
	void SetState(FSMState* newState);
	
	typedef std::pair<FSMTransition*, FSMState*> TransitionStatePair;
	typedef std::vector<TransitionStatePair> Transitions;

	map<FSMState*, Transitions> m_Transitions;
	FSMState* m_pCurrentState;
	IExamInterface* m_pInterface;
};

