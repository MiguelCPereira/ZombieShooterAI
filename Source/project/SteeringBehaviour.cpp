#include "stdafx.h"
#include "SteeringBehaviour.h"
#include <Exam_HelperStructs.h>


//SEEK
//****
SteeringPlugin_Output Seek::CalculateSteering(const AgentInfo& agentInfo)
{
	SteeringPlugin_Output steering = {}; // Create steering variable

	steering.LinearVelocity = (m_Target).Position - agentInfo.Position; // Desired Direction
	steering.LinearVelocity.Normalize(); // Normalize Vector
	steering.LinearVelocity *= agentInfo.MaxLinearSpeed; // Rescale to Max Speed

	return steering;
}

//PURSUIT (base> SEEK)
//****
SteeringPlugin_Output Pursuit::CalculateSteering(const AgentInfo& agentInfo)
{
	float targetDistance = Distance(agentInfo.Position, m_Target.Position); // Calculate the distance between the agen and the target
	float predictionTime = targetDistance / agentInfo.MaxLinearSpeed; // Calculate the prediction time by diving the distance with the max speed
	Elite::Vector2 targetDirection = m_Target.LinearVelocity.GetNormalized(); // Get the direction vector to the target

	TargetData predictedTarget = TargetData(m_Target.Position + (targetDirection * predictionTime)); // Make a new target with the current target position + it's direction multiplied by the prediction time
	m_Target = predictedTarget; // Set the current target to the new target

	return Seek::CalculateSteering(agentInfo);
}

//FLEE
//****
SteeringPlugin_Output Flee::CalculateSteering(const AgentInfo& agentInfo)
{
	float targetDistance = Distance(agentInfo.Position, m_Target.Position); // Calculate distance between the agent and the target

	//if (targetDistance > m_FleeRadius) // If the distance's bigger the the FleeRadius, stop moving
	//	return SteeringPlugin_Output{};

	SteeringPlugin_Output flee = Seek::CalculateSteering(agentInfo); // Make flee a seek
	flee.LinearVelocity = flee.LinearVelocity * -1.f; // Invert the LinearVelocity, so the agent goes away from the target

	return flee;
}

//EVADE (base> FLEE)
//****
SteeringPlugin_Output Evade::CalculateSteering(const AgentInfo& agentInfo)
{
	float targetDistance = Elite::Distance(agentInfo.Position, m_Target.Position); // Calculate the distance between the agent and the target

	if (targetDistance > m_EvadeRadius) // If the distance's bigger then the EvadeRadius, stop moving
		return SteeringPlugin_Output{};

	const float predictionTime = targetDistance / agentInfo.MaxLinearSpeed; // Calculate the prediction time by dividing the distance with the max speed
	const Elite::Vector2 targetDirection = m_Target.LinearVelocity.GetNormalized(); // Get the target's normalized direction vector

	const TargetData predictedTarget = TargetData(m_Target.Position + (targetDirection * predictionTime)); // Make a new target with the current target position + it's direction multiplied by the prediction time
	m_Target = predictedTarget; // Set the current target to the new target

	return Flee::CalculateSteering(agentInfo);
}

//ARRIVE
//****
SteeringPlugin_Output Arrive::CalculateSteering(const AgentInfo& agentInfo)
{
	SteeringPlugin_Output arrive = {}; // Create steering variable
	SteeringPlugin_Output target = {}; // Create target variable

	arrive.LinearVelocity = (m_Target).Position - agentInfo.Position; // Calculate Distance Vector
	const float distance{ Distance(agentInfo.Position, m_Target.Position) }; // Calculate Actual Distance
	float speed{};

	if (distance < m_TargetRadius) // If the distance is inferior to the target radius, he arrived - so he stops
	{
		return SteeringPlugin_Output{};
	}
	else
	{
		if (distance > m_SlowRadius) // If the distance is inferior to the slow radius, he goes full speed
		{
			speed = agentInfo.MaxLinearSpeed; // Define Speed to Max Speed
		}
		else
		{
			speed = agentInfo.MaxLinearSpeed * distance / m_SlowRadius; // Define Speed according to distance
		}

		arrive.LinearVelocity.Normalize(); // Normalize the Vector
		arrive.LinearVelocity *= speed; // Rescale to Desired Speed

		if (arrive.LinearVelocity.Magnitude() > agentInfo.MaxLinearSpeed) // If the calculated speed is bigger than max speed, make it max
		{
			arrive.LinearVelocity.Normalize();
			arrive.LinearVelocity *= agentInfo.MaxLinearSpeed;
		}
	}

	return arrive;
}

//FACE
//****
SteeringPlugin_Output Face::CalculateSteering(const AgentInfo& agentInfo)
{
	SteeringPlugin_Output face = {};
	face.AutoOrient = false;

	auto currentRotation = agentInfo.Orientation;
	if (currentRotation > 6.f)
		currentRotation -= 6.f * (int(currentRotation) / 6);

	if (currentRotation < 0.f)
		currentRotation += 6.f * (int(-currentRotation) / 6 + 1);

	float currentRotationAngle;
	if (currentRotation != 0.f)
	{
		currentRotationAngle = 360.f * currentRotation / 6.f - 90.f;
		if (currentRotationAngle < 0.f)
			currentRotationAngle += 360.f;
	}
	else currentRotationAngle = 270.f;

	
	const auto desiredDirection = m_Target.Position - agentInfo.Position;
	const Elite::Vector2 currentDirection{ cos(Elite::ToRadians(currentRotationAngle)), sin(Elite::ToRadians(currentRotationAngle)) };

	float angleBetweenVectors = atan2(currentDirection.x * desiredDirection.y - desiredDirection.x * currentDirection.y, currentDirection.x * desiredDirection.x + currentDirection.y * desiredDirection.y) * 180.f / M_PI;

	if (angleBetweenVectors > 180.f)
		angleBetweenVectors = - 360.f + angleBetweenVectors;

	if(angleBetweenVectors < -180.f)
		angleBetweenVectors = 360.f + angleBetweenVectors;
	
	face.AngularVelocity = angleBetweenVectors;
	

	return face;
}

//WANDER (base> SEEK)
//******
SteeringPlugin_Output Wander::CalculateSteering(const AgentInfo& agentInfo)
{
	SteeringPlugin_Output wander = {}; // Create steering variable

	Elite::Vector2 offsetVector = agentInfo.LinearVelocity.GetNormalized(); // Define the vector of the agent's velocity and normalize it to get just the direction
	offsetVector *= m_Offset; // Multiply that vector by the m_Offset to get the vector between the agent and the center of the circle
	m_WanderTarget = agentInfo.Position + offsetVector; // Add the agent position to the vector to get the center of the circle
	m_WanderAngle += Elite::randomFloat(-m_AngleChange, m_AngleChange); // Define m_WanderAngle between the max and min values of m_AngleChange
	Elite::Vector2 displacementVector = Elite::Vector2(cos(m_WanderAngle), sin(m_WanderAngle)) * m_Radius; // Calculate the vector between the circle center and the final target (with the calculated angle)
	m_Target = m_WanderTarget + displacementVector; // Add the circle center position to the displacement vector to get the final target


	wander = Seek::CalculateSteering(agentInfo); // Make a seek to the final target

	return wander;
}
