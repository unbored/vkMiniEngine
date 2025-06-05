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

#include <array>

#include <glm/glm.hpp>

class Color
{
public:
    Color( ) : m_value(glm::vec4(1.0f)) {}
    //Color( glm::vec4 vec );
    Color( const glm::vec4& vec );
    Color( float r, float g, float b, float a = 1.0f );
    Color( uint16_t r, uint16_t g, uint16_t b, uint16_t a = 255, uint16_t bitDepth = 8 );
    explicit Color( uint32_t rgbaLittleEndian );
        
    float R() const { return m_value.r; }
    float G() const { return m_value.g; }
    float B() const { return m_value.b; }
    float A() const { return m_value.a; }

    bool operator==( const Color& rhs ) const { return m_value == rhs.m_value; }
    bool operator!=( const Color& rhs ) const { return m_value != rhs.m_value; }

    void SetR( float r ) { m_value.r = r; }
    void SetG( float g ) { m_value.g = g; }
    void SetB( float b ) { m_value.b = b; }
    void SetA( float a ) { m_value.a = a; }

    float* GetPtr( void ) { return reinterpret_cast<float*>(this); }
    float& operator[]( int idx ) { return GetPtr()[idx]; }

    std::array<float, 4> GetArray() const { return { m_value.r, m_value.g, m_value.b, m_value.a }; }

    void SetRGB( float r, float g, float b ) { m_value = glm::vec4(r, g, b, m_value.a); }

    Color ToSRGB() const;
    Color FromSRGB() const;
    Color ToREC709() const;
    Color FromREC709() const;

    // Probably want to convert to sRGB or Rec709 first
    uint32_t R10G10B10A2() const;
    uint32_t R8G8B8A8() const;

    // Pack an HDR color into 32-bits
    uint32_t R11G11B10F(bool RoundToEven=false) const;
    uint32_t R9G9B9E5() const;

    operator glm::vec4() const { return m_value; }

private:
    glm::vec4 m_value;
};

inline Color Max( Color a, Color b ) { return glm::max(glm::vec4(a), glm::vec4(b)); }
inline Color Min( Color a, Color b ) { return glm::min(glm::vec4(a), glm::vec4(b)); }
inline Color Clamp( Color x, Color a, Color b ) { return glm::clamp(glm::vec4(x), glm::vec4(a), glm::vec4(b)); }


//inline Color::Color(glm::vec4 vec )
//{
//    m_value = vec;
//}

inline Color::Color( const glm::vec4& vec )
{
    m_value = vec;
}

inline Color::Color( float r, float g, float b, float a )
{
    m_value = glm::vec4(r, g, b, a);
}

inline Color::Color( uint16_t r, uint16_t g, uint16_t b, uint16_t a, uint16_t bitDepth )
{
    m_value = glm::vec4(r, g, b, a) / (float)((1 << bitDepth) - 1);
}

inline Color::Color( uint32_t u32 )
{
    float r = (float)((u32 >>  0) & 0xFF);
    float g = (float)((u32 >>  8) & 0xFF);
    float b = (float)((u32 >> 16) & 0xFF);
    float a = (float)((u32 >> 24) & 0xFF);
    m_value = glm::vec4(r, g, b, a) / 255.0f;
}

inline Color Color::ToSRGB( void ) const
{
    glm::vec4 T = glm::clamp(m_value, 0.0f, 1.0f);
    glm::vec4 result = glm::pow(T, glm::vec4(1.0f / 2.4f)) * 1.055f - 0.055f;
    result = glm::mix(result, T * 12.92f, glm::lessThan(T, glm::vec4(0.0031308f)));
    result.a = T.a;
    return result;
}

inline Color Color::FromSRGB( void ) const
{
    glm::vec4 T = glm::clamp(m_value, 0.0f, 1.0f);
    glm::vec4 result = glm::pow((T + 0.055f) / 1.055f, glm::vec4(2.4f));
    result = glm::mix(result, T / 12.92f, glm::lessThan(T, glm::vec4(0.0031308f)));
    result.a = T.a;
    return result;
}

inline Color Color::ToREC709( void ) const
{
    glm::vec4 T = glm::clamp(m_value, 0.0f, 1.0f);
    glm::vec4 result = glm::pow(T, glm::vec4(0.45f)) * 1.099f - 0.099f;
    result = glm::mix(result, T * 4.5f, glm::lessThan(T, glm::vec4(0.0018f)));
    result.a = T.a;
    return result;
}

inline Color Color::FromREC709( void ) const
{
    glm::vec4 T = glm::clamp(m_value, 0.0f, 1.0f);
    glm::vec4 result = glm::pow((T + 0.099f) / 1.099f, glm::vec4(1.0f / 0.45f));
    result = glm::mix(result, T / 4.5f, glm::lessThan(T, glm::vec4(0.0081f)));
    result.a = T.a;
    return result;
}

inline uint32_t Color::R10G10B10A2( void ) const
{
    glm::vec4 result = glm::round(glm::clamp(m_value, 0.0f, 1.0f) * glm::vec4(1023.0f, 1023.0f, 1023.0f, 3.0f));
    uint32_t r = (uint32_t)result.r;
    uint32_t g = (uint32_t)result.g;
    uint32_t b = (uint32_t)result.b;
    uint32_t a = (uint32_t)result.a >> 8;
    return a << 30 | b << 20 | g << 10 | r;
}

inline uint32_t Color::R8G8B8A8( void ) const
{
    glm::vec4 result = glm::round(glm::clamp(m_value, 0.0f, 1.0f) * glm::vec4(255.0f));
    uint32_t r = (uint32_t)result.r;
    uint32_t g = (uint32_t)result.g;
    uint32_t b = (uint32_t)result.b;
    uint32_t a = (uint32_t)result.a;
    return a << 24 | b << 16 | g << 8 | r;
}
