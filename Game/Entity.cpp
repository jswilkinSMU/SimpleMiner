#include "Game/Entity.hpp"
#include "Game/Game.h"
#include "Game/World.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Math/MathUtils.h"
#include "Engine/Core/DebugRender.hpp"

Entity::Entity(Game* owner, Vec3 const& position)
	:m_theGame(owner), m_position(position)
{
}

Entity::~Entity()
{
}

Mat44 Entity::GetModelToWorldTransform() const
{
	Mat44 modelToWorldMatrix;
	modelToWorldMatrix.SetTranslation3D(m_position);
	EulerAngles orientation;
	orientation.m_yawDegrees = m_orientation.m_yawDegrees;
	modelToWorldMatrix.Append(orientation.GetAsMatrix_IFwd_JLeft_KUp());
	return modelToWorldMatrix;
}

void Entity::UpdatePhysics(float deltaSeconds)
{
	if (m_physicsMode == PhysicsMode::WALKING)
	{
		m_gravity = -9.8f;

		// Applying gravity when walking
		if (!m_isGrounded)
		{
			m_velocity.z += m_gravity * deltaSeconds;
		}

		// Apply XY friction
		float friction = m_isGrounded ? m_groundFriction : m_airFriction;

		Vec2 velocityXY = m_velocity.GetXY();
		float speed = velocityXY.GetLength();
		if (speed > 0.f)
		{
			float drop = friction * deltaSeconds;
			speed = GetMax(speed - drop, 0.f);
			velocityXY = velocityXY.GetNormalized() * speed;
			m_velocity.x = velocityXY.x;
			m_velocity.y = velocityXY.y;
		}

		// Resolve collision
		ResolveCollisions(deltaSeconds);
	}
	else if (m_physicsMode == PhysicsMode::FLYING)
	{
		// No gravity
		m_gravity = 0.f;

		// Apply XY friction
		float friction = m_isGrounded ? m_groundFriction : m_airFriction;

		Vec2 velocityXY = m_velocity.GetXY();
		float speed = velocityXY.GetLength();
		if (speed > 0.f)
		{
			float drop = friction * deltaSeconds;
			speed = GetMax(speed - drop, 0.f);
			velocityXY = velocityXY.GetNormalized() * speed;
			m_velocity.x = velocityXY.x;
			m_velocity.y = velocityXY.y;
		}

		// Resolve collision
		ResolveCollisions(deltaSeconds);
	}
	else if (m_physicsMode == PhysicsMode::NOCLIP)
	{
		// No gravity
		m_gravity = 0.f;
		
		// Apply XY friction
		float friction = m_isGrounded ? m_groundFriction : m_airFriction;

		Vec2 velocityXY = m_velocity.GetXY();
		float speed = velocityXY.GetLength();
		if (speed > 0.f)
		{
			float drop = friction * deltaSeconds;
			speed = GetMax(speed - drop, 0.f);
			velocityXY = velocityXY.GetNormalized() * speed;
			m_velocity.x = velocityXY.x;
			m_velocity.y = velocityXY.y;
		}

		// Move with velocity
		Vec3 oldPosition = m_position;
		m_position += m_velocity * deltaSeconds;
	}
}

void Entity::UpdateIsGrounded()
{
	m_isGrounded = false;
	float inset = 0.001f;

	AABB3 box = m_physicsBody;
	box.Translate(m_position);
	box.m_mins += Vec3(inset, inset, inset);
	box.m_maxs -= Vec3(inset, inset, inset);

	Vec3 b0(box.m_mins.x, box.m_mins.y, box.m_mins.z);
	Vec3 b1(box.m_maxs.x, box.m_mins.y, box.m_mins.z);
	Vec3 b2(box.m_mins.x, box.m_maxs.y, box.m_mins.z);
	Vec3 b3(box.m_maxs.x, box.m_maxs.y, box.m_mins.z);

	Vec3 bottomCorners[4] = { b0, b1, b2, b3 };

	Vec3 down = -Vec3::ZAXE;
	float rayLength = 0.05f;

	for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
	{
		Vec3 start = bottomCorners[cornerIndex] + Vec3(0, 0, 0.02f);

		GameRaycastResult3D raycastResult = m_theGame->m_currentWorld->RaycastVsBlocks(start, down, rayLength);

		// Debug drawing ground rays
		Vec3 end = start + down * rayLength;
		Rgba8 color = raycastResult.m_didImpact ? Rgba8(0, 255, 0) : Rgba8(255, 0, 0);

		if (m_isCollisionDebugOn)
		{
			DebugAddWorldArrow(start, end, 0.01f, 0.f, color, color);
		}

		if (raycastResult.m_didImpact)
		{
			m_isGrounded = true;
			return;
		}
	}
}

void Entity::ResolveCollisions(float deltaSeconds) 
{
	if (m_velocity.IsNearlyZero())
	{
		return;
	}

	Vec3 deltaPosition = m_velocity * deltaSeconds;
	float moveDistance = deltaPosition.GetLength();

	if (moveDistance <= 0.00001f)
	{
		return;
	}

	Vec3 direction = deltaPosition / moveDistance;
	float inset = 0.001f;

	AABB3 box = m_physicsBody;   
	box.Translate(m_position);
	box.m_mins += Vec3(inset, inset, inset);
	box.m_maxs -= Vec3(inset, inset, inset);

	std::vector<Vec3> corners;
	corners.reserve(12);

	// Bottom 4
	corners.emplace_back(box.m_mins.x, box.m_mins.y, box.m_mins.z);
	corners.emplace_back(box.m_maxs.x, box.m_mins.y, box.m_mins.z);
	corners.emplace_back(box.m_mins.x, box.m_maxs.y, box.m_mins.z);
	corners.emplace_back(box.m_maxs.x, box.m_maxs.y, box.m_mins.z);

	// Middle 4
	float midZ = (box.m_mins.z + box.m_maxs.z) * 0.5f;
	corners.emplace_back(box.m_mins.x, box.m_mins.y, midZ);
	corners.emplace_back(box.m_maxs.x, box.m_mins.y, midZ);
	corners.emplace_back(box.m_mins.x, box.m_maxs.y, midZ);
	corners.emplace_back(box.m_maxs.x, box.m_maxs.y, midZ);

	// Top 4
	corners.emplace_back(box.m_mins.x, box.m_mins.y, box.m_maxs.z);
	corners.emplace_back(box.m_maxs.x, box.m_mins.y, box.m_maxs.z);
	corners.emplace_back(box.m_mins.x, box.m_maxs.y, box.m_maxs.z);
	corners.emplace_back(box.m_maxs.x, box.m_maxs.y, box.m_maxs.z);

	float earliestX = 1.f, earliestY = 1.f, earliestZ = 1.f;
	bool hitX = false, hitY = false, hitZ = false;
	Vec3 normX, normY, normZ;

	for (Vec3 const& corner : corners)
	{
		GameRaycastResult3D raycastResult = m_theGame->m_currentWorld->RaycastVsBlocks(corner, direction, moveDistance + 0.01f);

		// Debug drawing ground rays
		Vec3 end = corner + direction * 0.1f;
		Rgba8 color = raycastResult.m_didImpact ? Rgba8(0, 255, 0) : Rgba8(255, 0, 0);

		if (m_isCollisionDebugOn)
		{
			DebugAddWorldArrow(corner, end, 0.01f, 0.f, color, color);
		}

		// Early check for impact
		if (!raycastResult.m_didImpact)
		{
			continue;
		}

		float time = raycastResult.m_impactDist / (moveDistance + 0.01f);
		if (time < 0.f || time > 1.f)
		{
			continue;
		}

		// Ignoring hits that are on the backface
		if (DotProduct3D(raycastResult.m_impactNormal, direction) > 0.f)
		{
			continue;
		}

		Vec3 normal = raycastResult.m_impactNormal;

		if (fabsf(normal.x) > fabsf(normal.y) && fabsf(normal.x) > fabsf(normal.z))
		{
			if (time < earliestX)
			{
				earliestX = time;
				normX = normal;
				hitX = true;
			}
		}
		else if (fabsf(normal.y) > fabsf(normal.z))
		{
			if (time < earliestY)
			{
				earliestY = time;
				normY = normal;
				hitY = true;
			}
		}
		else
		{
			if (time < earliestZ)
			{
				earliestZ = time;
				normZ = normal;
				hitZ = true;
			}
		}
	}

	Vec3 resolved = deltaPosition;

	if (hitX)
	{
		resolved.x = 0.f;
		m_velocity.x = 0.f;
	}
	if (hitY)
	{
		resolved.y = 0.f;
		m_velocity.y = 0.f;
	}
	if (hitZ)
	{
		resolved.z = 0.f;
		m_velocity.z = 0.f;
	}

	m_position += resolved;
}