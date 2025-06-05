//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdint.h>

#define INLINE inline

namespace Math
{
template <typename T> inline T AlignUpWithMask(T value, size_t mask) { return (T)(((size_t)value + mask) & ~mask); }

template <typename T> inline T AlignDownWithMask(T value, size_t mask) { return (T)((size_t)value & ~mask); }

template <typename T> inline T AlignUp(T value, size_t alignment) { return AlignUpWithMask(value, alignment - 1); }

template <typename T> inline T AlignDown(T value, size_t alignment) { return AlignDownWithMask(value, alignment - 1); }

template <typename T> inline bool IsAligned(T value, size_t alignment)
{
    return 0 == ((size_t)value & (alignment - 1));
}

template <typename T> inline T DivideByMultiple(T value, size_t alignment)
{
    return (T)((value + alignment - 1) / alignment);
}

template <typename T> inline bool IsPowerOfTwo(T value) { return 0 == (value & (value - 1)); }

template <typename T> inline bool IsDivisible(T value, T divisor) { return (value / divisor) * divisor == value; }

inline uint8_t Log2(uint64_t value)
{
    unsigned long mssb; // most significant set bit
    unsigned long lssb; // least significant set bit

    // If perfect power of two (only one set bit), return index of bit.  Otherwise round up
    // fractional log by adding 1 to most signicant set bit's index.
    mssb = glm::findMSB(value);
    lssb = glm::findLSB(value);
    if (mssb > 0 && lssb > 0)
        // if (_BitScanReverse64(&mssb, value) > 0 && _BitScanForward64(&lssb, value) > 0)
        return uint8_t(mssb + (mssb == lssb ? 0 : 1));
    else
        return 0;
}

template <typename T> inline T AlignPowerOfTwo(T value) { return value == 0 ? 0 : 1 << Log2(value); }

INLINE glm::vec4 SplatZero() { return glm::vec4(0); }

// INLINE glm::vec4 SplatOne() { return glm::vec4(1.0); }
INLINE glm::vec3 CreateXUnitVector() { return glm::vec3(1.0, 0.0, 0.0); }
INLINE glm::vec3 CreateYUnitVector() { return glm::vec3(0.0, 1.0, 0.0); }
INLINE glm::vec3 CreateZUnitVector() { return glm::vec3(0.0, 0.0, 1.0); }
// INLINE glm::vec3 CreateWUnitVector() { return glm::vec4(0.0, 0.0, 0.0, 1.0); }
// INLINE glm::vec4 SetWToZero(glm::vec4 vec) { return glm::vec4(glm::vec3(vec), 0.0); }
// INLINE glm::vec4 SetWToOne(glm::vec4 vec) { return glm::vec4(glm::vec3(vec), 1.0); }
// INLINE glm::vec4 SetWToZero(glm::vec3 vec) { return glm::vec4(vec, 0.0); }
// INLINE glm::vec4 SetWToOne(glm::vec3 vec) { return glm::vec4(vec, 1.0); }
INLINE glm::mat3 MakeXRotation(float angle) { return glm::rotate(glm::mat4(1.0), angle, CreateXUnitVector()); }
INLINE glm::mat3 MakeYRotation(float angle) { return glm::rotate(glm::mat4(1.0), angle, CreateYUnitVector()); }
INLINE glm::mat3 MakeZRotation(float angle) { return glm::rotate(glm::mat4(1.0), angle, CreateZUnitVector()); }

INLINE float LengthSquare(glm::vec3 vec) { return glm::dot(vec, vec); }

enum EZeroTag
{
    kZero,
    kOrigin
};
enum EIdentityTag
{
    kOne,
    kIdentity
};
// enum EXUnitVector { kXUnitVector };
// enum EYUnitVector { kYUnitVector };
// enum EZUnitVector { kZUnitVector };
// enum EWUnitVector { kWUnitVector };

} // namespace Math
