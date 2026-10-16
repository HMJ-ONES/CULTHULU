#include "camera/CameraSystem.h"

#include <cmath>

namespace cultulhu {

CameraSystem::CameraSystem() = default;

void CameraSystem::switchCamera() {
    mode_ = (mode_ == CameraMode::FirstPerson) ? CameraMode::ThirdPerson
                                              : CameraMode::FirstPerson;
}

void CameraSystem::setYawPitch(float yaw, float pitch) {
    yaw_ = yaw;
    // Clamp pitch to avoid flipping.
    const float lim = 1.55f;
    pitch_ = pitch < -lim ? -lim : (pitch > lim ? lim : pitch);
}

CameraPose CameraSystem::poseFor(Vec3 avatarPos, float avatarYaw) const {
    CameraPose pose;
    if (mode_ == CameraMode::FirstPerson) {
        pose.eye = {avatarPos.x, avatarPos.y + eyeHeight_, avatarPos.z};
        const float cy = std::cos(yaw_), sy = std::sin(yaw_);
        const float cp = std::cos(pitch_);
        Vec3 dir{cp * cy, std::sin(pitch_), cp * sy};
        pose.lookAt = {pose.eye.x + dir.x, pose.eye.y + dir.y,
                       pose.eye.z + dir.z};
    } else {
        // Over-shoulder boom follow: behind the avatar, offset laterally.
        const float cy = std::cos(avatarYaw), sy = std::sin(avatarYaw);
        Vec3 back{-cy * followDistance_, 0.0f, -sy * followDistance_};
        Vec3 right{-sy * shoulderOffset_, 0.0f, cy * shoulderOffset_};
        pose.eye = {avatarPos.x + back.x + right.x,
                    avatarPos.y + eyeHeight_ + 1.0f,
                    avatarPos.z + back.z + right.z};
        pose.lookAt = {avatarPos.x, avatarPos.y + eyeHeight_, avatarPos.z};
    }
    return pose;
}

} // namespace cultulhu
