#pragma once
enum class Event
{
	MedkitPickedUp,
	FoodPickedUp,
	PistolPickedUp
};

class Observer
{
public:
	virtual ~Observer() = default;
	virtual void OnNotify(const Event& event) = 0;
};

