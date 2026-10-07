#pragma once

#include <cmath>

namespace cultulhu {

// Minimal 3D vector, engine-agnostic (Unreal binding can map to FVector).
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float distance(const Vec3& o) const { return (*this - o).length(); }

    Vec3 normalized() const {
        float l = length();
        return (l > 1e-6f) ? (*this) * (1.0f / l) : Vec3();
    }
};

} // namespace cultulhu
