#include "stdafx.h"
#include "FiniteStateMachine.h"

#include <IExamInterface.h>

FiniteStateMachine::FiniteStateMachine(FSMState* startState, IExamInterface* pInterface)
    : m_Transitions()
	, m_pCurrentState(nullptr)
	, m_pInterface(pInterface)
{
    SetState(startState);
}

void FiniteStateMachine::AddTransition(FSMState* startState, FSMState* toState, FSMTransition* transition)
{
    auto it = m_Transitions.find(startState);
    if (it == m_Transitions.end())
    {
        m_Transitions[startState] = Transitions();
    }

    m_Transitions[startState].push_back(std::make_pair(transition, toState));
}

SteeringPlugin_Output FiniteStateMachine::Update(float deltaTime)
{
    auto it = m_Transitions.find(m_pCurrentState);
    if (it != m_Transitions.end())
    {
        for (TransitionStatePair& transPair : it->second)
        {
            transPair.first->Update(deltaTime, m_pInterface);
            if (transPair.first->ToTransition(m_pInterface))
            {
                SetState(transPair.second);
                break;
            }
        }
    }

    if (m_pCurrentState)
        return m_pCurrentState->Update(deltaTime, m_pInterface);

    return SteeringPlugin_Output{};
}

void FiniteStateMachine::SetState(FSMState* newState)
{
    if (m_pCurrentState)
        m_pCurrentState->OnExit(m_pInterface);
	
    m_pCurrentState = newState;
	
    if (m_pCurrentState)
    {
        std::cout << "Entering state: " << typeid(*m_pCurrentState).name() << std::endl;
        m_pCurrentState->OnEnter(m_pInterface);
    }
}
