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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
//#include <xmmintrin.h>

namespace Utility
{
inline void Print(const char *msg) { printf("%s", msg); }
// inline void Print(const wchar_t *msg) { wprintf(L"%ws", msg); }

inline void Printf(const char *format, ...)
{
    char buffer[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, 256, format, ap);
    va_end(ap);
    Print(buffer);
}

// inline void Printf(const wchar_t *format, ...)
// {
//     wchar_t buffer[256];
//     va_list ap;
//     va_start(ap, format);
//     vswprintf(buffer, 256, format, ap);
//     va_end(ap);
//     Print(buffer);
// }

#ifndef NDEBUG
inline void PrintSubMessage(const char *format, ...)
{
    Print("--> ");
    char buffer[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, 256, format, ap);
    va_end(ap);
    Print(buffer);
    Print("\n");
}
// inline void PrintSubMessage(const wchar_t *format, ...)
// {
//     Print("--> ");
//     wchar_t buffer[256];
//     va_list ap;
//     va_start(ap, format);
//     vswprintf(buffer, 256, format, ap);
//     va_end(ap);
//     Print(buffer);
//     Print("\n");
// }
inline void PrintSubMessage(void) {}
#endif

// std::wstring UTF8ToWideString( const std::string& str );
// std::string WideStringToUTF8( const std::wstring& wstr );
std::string ToLower(const std::string &str);
// std::wstring ToLower(const std::wstring& str);
std::string GetBasePath(const std::string &str);
// std::wstring GetBasePath(const std::wstring& str);
std::string RemoveBasePath(const std::string &str);
// std::wstring RemoveBasePath(const std::wstring& str);
std::string GetFileExtension(const std::string &str);
// std::wstring GetFileExtension(const std::wstring& str);
std::string RemoveExtension(const std::string &str);
// std::wstring RemoveExtension(const std::wstring& str);

} // namespace Utility

#ifdef ERROR
#undef ERROR
#endif
#ifdef ASSERT
#undef ASSERT
#endif
#ifdef HALT
#undef HALT
#endif

#define HALT(...) ERROR(__VA_ARGS__) raise(SIGABRT);

#ifdef NDEBUG

#define ASSERT(isTrue, ...) (void)(isTrue)
#define WARN_ONCE_IF(isTrue, ...) (void)(isTrue)
#define WARN_ONCE_IF_NOT(isTrue, ...) (void)(isTrue)
#define ERROR(msg, ...)
#define DEBUGPRINT(msg, ...)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (0)
#define ASSERT_SUCCEEDED(hr, ...) (void)(hr)

#else // !NDEBUG

#define STRINGIFY(x) #x
#define STRINGIFY_BUILTIN(x) STRINGIFY(x)
#define ASSERT(isFalse, ...)                                                                                           \
    if (!(bool)(isFalse))                                                                                              \
    {                                                                                                                  \
        Utility::Print("\nAssertion failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n");   \
        Utility::PrintSubMessage("\'" #isFalse "\' is false");                                                         \
        Utility::PrintSubMessage(__VA_ARGS__);                                                                         \
        Utility::Print("\n");                                                                                          \
        raise(SIGABRT);                                                                                                \
    }

#define ASSERT_SUCCEEDED(hr, ...)                                                                                      \
    if (!(hr))                                                                                                         \
    {                                                                                                                  \
        Utility::Print("\nHRESULT failed in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n");     \
        Utility::PrintSubMessage("hr = 0x%08X", hr);                                                                   \
        Utility::PrintSubMessage(__VA_ARGS__);                                                                         \
        Utility::Print("\n");                                                                                          \
        raise(SIGABRT);                                                                                                \
    }

#define WARN_ONCE_IF(isTrue, ...)                                                                                      \
    {                                                                                                                  \
        static bool s_TriggeredWarning = false;                                                                        \
        if ((bool)(isTrue) && !s_TriggeredWarning)                                                                     \
        {                                                                                                              \
            s_TriggeredWarning = true;                                                                                 \
            Utility::Print("\nWarning issued in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n"); \
            Utility::PrintSubMessage("\'" #isTrue "\' is true");                                                       \
            Utility::PrintSubMessage(__VA_ARGS__);                                                                     \
            Utility::Print("\n");                                                                                      \
        }                                                                                                              \
    }

#define WARN_ONCE_IF_NOT(isTrue, ...) WARN_ONCE_IF(!(isTrue), __VA_ARGS__)

#define ERROR(...)                                                                                                     \
    Utility::Print("\nError reported in " STRINGIFY_BUILTIN(__FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n");         \
    Utility::PrintSubMessage(__VA_ARGS__);                                                                             \
    Utility::Print("\n");

#define DEBUGPRINT(msg, ...) Utility::Printf(msg "\n", ##__VA_ARGS__);

#endif

#define BreakIfFailed(hr)                                                                                              \
    if (!(hr))                                                                                                         \
    raise(SIGABRT)

// void SIMDMemCopy( void* __restrict Dest, const void* __restrict Source, size_t NumQuadwords );
// void SIMDMemFill( void* __restrict Dest, __m128 FillVector, size_t NumQuadwords );
