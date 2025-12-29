#pragma once
#include "Game/Entity.hpp"
#include "Engine/Renderer/Camera.h"
// -----------------------------------------------------------------------------
class Player;
// -----------------------------------------------------------------------------
enum class CameraMode
{
	NONE = -1,
	FIRST_PERSON,
	OVER_SHOULDER,
	FIXED_ANGLE_TRACKING,
	SPECTATOR_FULL,
	SPECTATOR_XY,
	INDEPENDENT,
	NUM_CAMERA_MODES
};
// -----------------------------------------------------------------------------
class GameCamera : public Entity
{
public:
	GameCamera(Game* owner, Vec3 const& position);
	~GameCamera();

	void Update(float deltaSeconds) override;
	void Render() const override;

	void CycleCameraMode();
	void AttachCameraTo(Player* target);
	Camera& GetRenderCamera();

private:
	void UpdateFirstPerson();
	void UpdateFixedAngleTracking();
	void UpdateOverShoulder();
	void UpdateSpectator(float deltaSeconds, bool xyOnly);
	void UpdateIndependent(float deltaSeconds);

public:
	CameraMode m_cameraMode = CameraMode::SPECTATOR_FULL;

private:
	Player* m_target = nullptr;
	Camera  m_renderCamera;
};