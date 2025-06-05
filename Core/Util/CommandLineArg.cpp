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
// Author:  Jack Elliott
//

#include <unordered_map>
#include <string>
#include <sstream>

namespace CommandLineArgs
{
    std::unordered_map<std::string, std::string> m_argumentMap;

    template<typename GetStringFunc>
    void GatherArgs(size_t numArgs, const GetStringFunc& pfnGet)
    {
        // the first arg is always the exe name and we are looing for key/value pairs
        if (numArgs > 1)
        {
            size_t i = 0;

            while (i <  numArgs)
            {
                const char* key = pfnGet(i);
                i++;
                if (i < numArgs && key[0] == '-')
                {
                    const char* strippedKey = key + 1;
                    m_argumentMap[strippedKey] = pfnGet(i);
                    i++;
                }
            }
        }
    }

    void Initialize(int argc, char** argv)
    {
        GatherArgs(size_t(argc), [argv](size_t i) -> const char* { return argv[i]; });
    }

    template<typename FunctionType>
    bool Lookup(const char* key, const FunctionType& pfnFunction)
    {
        const auto found = m_argumentMap.find(key);
        if (found != m_argumentMap.end())
        {
            pfnFunction(found->second);
            return true;
        }

        return false;
    }

    bool GetInteger(const char* key, uint32_t& value)
    {
        return Lookup(key, [&value](std::string& val)
        {
            value = std::stoi(val);
        });
    }

    bool GetFloat(const char* key, float& value)
    {
        return Lookup(key, [&value](std::string& val)
        {
            value = (float)atof(val.c_str());
        });
    }

    bool GetString(const char* key, std::string& value)
    {
        return Lookup(key, [&value](std::string& val)
        {
            value = val;
        });
    }
}