#include "Game/GameCamera.hpp"
#include "Game/Player.hpp"
#include "Game/World.hpp"
#include "Game/Game.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Core/EngineCommon.h"
#include "Engine/Math/MathUtils.h"

GameCamera::GameCamera(Game* owner, Vec3 const& position)
	:Entity(owner, position)
{
	m_orientation = EulerAngles(-45.f, 30.f, 0.f);

	Mat44 cameraToRender(Vec3::ZAXE, -Vec3::XAXE, Vec3::YAXE, Vec3::ZERO);
	m_renderCamera.SetCameraToRenderTransform(cameraToRender);

	m_renderCamera.SetPerspectiveView(2.f, 60.f, 0.01f, 10000.f);
}

GameCamera::~GameCamera()
{
}

void GameCamera::Update(float deltaSeconds)
{
	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
	m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

	if (!m_target)
	{
		return;
	}

	switch (m_cameraMode)
	{
		case CameraMode::FIRST_PERSON: UpdateFirstPerson(); break;
		case CameraMode::FIXED_ANGLE_TRACKING: UpdateFixedAngleTracking(); break;
		case CameraMode::OVER_SHOULDER: UpdateOverShoulder(); break;
		case CameraMode::SPECTATOR_FULL: UpdateSpectator(deltaSeconds, false); break;
		case CameraMode::SPECTATOR_XY: UpdateSpectator(deltaSeconds, true); break;
		case CameraMode::INDEPENDENT: UpdateIndependent(deltaSeconds); break;
	}
}

void GameCamera::Render() const
{
}

void GameCamera::CycleCameraMode()
{
	int cameraMode = static_cast<int>(m_cameraMode);
	cameraMode = (cameraMode + 1) % static_cast<int>(CameraMode::NUM_CAMERA_MODES);
	m_cameraMode = static_cast<CameraMode>(cameraMode);

	if (m_cameraMode == CameraMode::SPECTATOR_FULL || m_cameraMode == CameraMode::SPECTATOR_XY)
	{
		AttachCameraTo(m_target);
	}
}

void GameCamera::AttachCameraTo(Player* target)
{
	m_target = target;
}

Camera& GameCamera::GetRenderCamera()
{
	return m_renderCamera;
}

void GameCamera::UpdateFirstPerson()
{
	m_position = m_target->GetEyePosition();
	m_orientation = m_target->GetPlayerOrientation();
	m_renderCamera.SetPositionAndOrientation(m_position, m_orientation);
}

void GameCamera::UpdateFixedAngleTracking()
{
	EulerAngles fixedAngles = EulerAngles(40.f, 30.f, 0.f);
	m_renderCamera.SetOrientation(fixedAngles);

	Mat44 cameraMatrix = fixedAngles.GetAsMatrix_IFwd_JLeft_KUp();
	Vec3 back = -cameraMatrix.GetIBasis3D();

	float distance = 10.f;
	Vec3 cameraPos = m_target->GetPlayerPosition() + back * distance;

	m_renderCamera.SetPosition(cameraPos);
}

void GameCamera::UpdateOverShoulder()
{
	m_renderCamera.SetOrientation(m_target->GetPlayerOrientation());
	Vec3 iBasis = m_renderCamera.GetOrientation().GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D();
	float back = 4.f;
	Vec3 cameraPosition = m_target->GetPlayerPosition() + Vec3(0.f, 0.f, 1.65f) - iBasis * back;

	// Raycast to check if our view is obstructed by a tree or any other solid blocks
	Vec3 rayStart = m_target->GetEyePosition();
	Vec3 rayDirection = (cameraPosition - rayStart).GetNormalized();
	float maxDist = (cameraPosition - rayStart).GetLength();

	GameRaycastResult3D raycastResult = m_theGame->m_currentWorld->RaycastVsBlocks(rayStart, rayDirection, maxDist);
	Vec3 targetCamPos = cameraPosition;

	if (raycastResult.m_didImpact)
	{
		targetCamPos = rayStart + rayDirection * (raycastResult.m_impactDist);
	}

	if (targetCamPos != cameraPosition)
	{
		m_position = Interpolate(m_position, targetCamPos, CAMERA_SMOOTHING);
	}
	else
	{
		m_position = cameraPosition;
	}

	m_renderCamera.SetPosition(m_position);
}

void GameCamera::UpdateSpectator(float deltaSeconds, bool xyOnly)
{
	// Yaw and Pitch with mouse
	m_orientation.m_yawDegrees += 0.075f * g_theInput->GetCursorClientDelta().x;
	m_orientation.m_pitchDegrees -= 0.075f * g_theInput->GetCursorClientDelta().y;

	float movementSpeed = 4.f;

	// Increase speed by a factor of 10
	if (g_theInput->IsKeyDown(KEYCODE_SHIFT))
	{
		movementSpeed *= 20.f;
	}

	Mat44 cameraMatrix = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();

	Vec3 iBasis = cameraMatrix.GetIBasis3D();
	Vec3 jBasis = cameraMatrix.GetJBasis3D();
	Vec3 kBasis = cameraMatrix.GetKBasis3D();

	if (xyOnly)
	{
		iBasis.z = 0.f;
		jBasis.z = 0.f;
		iBasis.Normalize();
		jBasis.Normalize();
	}

	// Move left or right
	if (g_theInput->IsKeyDown('A'))
	{
		m_position += movementSpeed * jBasis * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('D'))
	{
		m_position += -movementSpeed * jBasis * deltaSeconds;
	}

	// Move Forward and Backward
	if (g_theInput->IsKeyDown('W'))
	{
		m_position += movementSpeed * iBasis * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('S'))
	{
		m_position += -movementSpeed * iBasis * deltaSeconds;
	}

	// Move Up and Down
	if (g_theInput->IsKeyDown('Q'))
	{
		m_position += movementSpeed * Vec3::ZAXE * deltaSeconds;
	}
	if (g_theInput->IsKeyDown('E'))
	{
		m_position += -movementSpeed * Vec3::ZAXE * deltaSeconds;
	}

	m_renderCamera.SetPositionAndOrientation(m_position, m_orientation);
}

void GameCamera::UpdateIndependent(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}
