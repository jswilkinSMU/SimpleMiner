#pragma once
#include "Game/GameCommon.h"
#include "Engine/Math/Vec3.h"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/AABB3.hpp"
// -----------------------------------------------------------------------------
class Game;
class Mat44;
// -----------------------------------------------------------------------------
enum class PhysicsMode
{
	NONE = -1,
	WALKING,
	FLYING,
	NOCLIP,
	NUM_PHYSICS_MODES
};
// -----------------------------------------------------------------------------
class Entity
{
public:
	Entity(Game* owner, Vec3 const& position);
	virtual ~Entity();

	virtual void Update(float deltaSeconds) = 0;
	virtual void Render() const = 0;
	virtual Mat44 GetModelToWorldTransform() const;

	void UpdatePhysics(float deltaSeconds);
	void UpdateIsGrounded();

	void ResolveCollisions(float deltaTime);
protected:
	Game* m_theGame = nullptr;
	Vec3  m_position = Vec3::ZERO;
	EulerAngles m_orientation = EulerAngles::ZERO;

	// Physics
	AABB3 m_physicsBody = AABB3(Vec3::ZERO, Vec3::ZERO);
	PhysicsMode m_physicsMode = PhysicsMode::WALKING;
	Vec3 m_velocity = Vec3::ZERO;
	bool m_isGrounded = false;
	float m_maxSpeedXY = 10.f;
	float m_groundAcceleration = 64.f;
	float m_airAcceleration = 10.f;
	float m_groundFriction = 15.f;
	float m_airFriction = 1.f;
	float m_gravity = -9.8f;
	float m_jumpImpulse = 5.5f;
	bool  m_isCollisionDebugOn = true;
};
// -----------------------------------------------------------------------------


