#pragma once

#include "core/Vec3.h"

namespace cultulhu {

// Switchable first/third person camera. Pure logic — no rendering. The engine
// binding (Unreal/Unity) reads poseFor() each frame and drives the real
// camera; this class owns the mode, the FP look angles, and the TP boom.
enum class CameraMode { FirstPerson, ThirdPerson };

inline const char* cameraModeName(CameraMode m) {
    return m == CameraMode::FirstPerson ? "FirstPerson" : "ThirdPerson";
}

// View parameters for one frame.
struct CameraPose {
    Vec3 eye;      // camera position
    Vec3 lookAt;   // point the camera looks toward
    float fov = 90.0f;
};

class CameraSystem {
public:
    CameraSystem();

    CameraMode mode() const { return mode_; }
    void setMode(CameraMode m) { mode_ = m; }
    void switchCamera(); // toggles FP <-> TP at runtime

    // First-person look state (radians). Yaw 0 faces +X.
    void setYawPitch(float yaw, float pitch);
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }

    // Third-person boom tuning.
    void setFollowDistance(float d) { followDistance_ = d; }
    void setShoulderOffset(float o) { shoulderOffset_ = o; }

    // Crosshair only makes sense in first person.
    bool crosshairVisible() const { return mode_ == CameraMode::FirstPerson; }

    // Compute the pose from the avatar's position and facing yaw.
    CameraPose poseFor(Vec3 avatarPos, float avatarYaw) const;

private:
    CameraMode mode_ = CameraMode::ThirdPerson;
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float followDistance_ = 6.0f;  // TP boom length
    float shoulderOffset_ = 1.2f;  // TP over-shoulder lateral offset
    float eyeHeight_ = 1.7f;
};

} // namespace cultulhu
