#pragma once
#include <Exam_HelperStructs.h>

/*=============================================================================*/
// Heavily inspired in the implementation from class
//		Copyright 2017-2018 Elite Engine
//		Authors: Matthieu Delaere, Thomas Goussaert
/*=============================================================================*/


struct AgentInfo;

struct TargetData
{
	Elite::Vector2 Position;
	Elite::Vector2 LinearVelocity;
	float AngularVelocity;

	TargetData(Elite::Vector2 position = Elite::ZeroVector2, Elite::Vector2 linearVel = Elite::ZeroVector2, float angularVel = 0.f)
		: Position(position)
		, LinearVelocity(linearVel)
		, AngularVelocity(angularVel)
	{}
};



class SteeringBehaviour
{
public:
	SteeringBehaviour() = default;
	virtual ~SteeringBehaviour() = default;

	virtual SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) = 0;

	//Seek Functions
	void SetTarget(const TargetData& target) { m_Target = target; }
	TargetData GetTarget() const { return m_Target; }

	template<class T, typename std::enable_if<std::is_base_of<SteeringBehaviour, T>::value>::type* = nullptr>
	T* As()
	{
		return static_cast<T*>(this);
	}

protected:
	TargetData m_Target;
};





class Seek : public SteeringBehaviour
{
public:
	Seek() = default;
	virtual ~Seek() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;
};

///////////////////////////////////////

class Pursuit : public Seek
{
public:
	Pursuit() = default;
	virtual ~Pursuit() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;
};

//////////////////////////

class Flee : public Seek
{
public:
	Flee() = default;
	virtual ~Flee() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;

private:
	//float m_FleeRadius = 15.f;
};

//////////////////////////

class Evade : public Flee
{
public:
	Evade() = default;
	virtual ~Evade() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;

	void SetEvadeRadius(float evasionRadius) { m_EvadeRadius = evasionRadius; }

private:
	float m_EvadeRadius = 15.f; // Agent will only evade once within this range
};

//////////////////////////

class Arrive : public SteeringBehaviour
{
public:
	Arrive() = default;
	virtual ~Arrive() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;

	void SetTargetRadius(float targetRadius) { m_TargetRadius = targetRadius; }
	void SetSlowRadius(float slowRadius) { m_SlowRadius = slowRadius; }

protected:
	float m_TargetRadius = 4.f; // Radius in which it recognizes as hitting the target and stops
	float m_SlowRadius = 10.f; // Radius in which it starts slowing down
};

//////////////////////////

class Face : public SteeringBehaviour
{
public:
	Face() = default;
	virtual ~Face() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;
};

//////////////////////////

class Wander : public Seek
{
public:
	Wander() = default;
	virtual ~Wander() = default;

	SteeringPlugin_Output CalculateSteering(const AgentInfo& agentInfo) override;

	void SetWanderOffset(float offset) { m_Offset = offset; }
	void SetWanderRadius(float radius) { m_Radius = radius; }
	void SetMaxAngleChange(float rad) { m_AngleChange = rad; }

protected:
	float m_Offset = 6.f; // Offset (Agent direction)
	float m_Radius = 4.f; // Wander Radius
	float m_AngleChange = Elite::ToRadians(45); // Max WanderAngle change per frame
	float m_WanderAngle = 0.f; // Internal

	Elite::Vector2 m_WanderTarget;
	float m_MaxJitterDistance = 1.f;
};
