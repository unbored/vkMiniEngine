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

#include "EngineProfiling.h"
#include "GpuTimeManager.h"
#include "SystemTime.h"
#include "Utility.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace EngineProfiling
{
bool Paused = false;
}

class GpuTimer
{
public:
    GpuTimer() { m_TimerIndex = GpuTimeManager::NewTimer(); }

    void Start(CommandContext &Context) { GpuTimeManager::StartTimer(Context, m_TimerIndex); }

    void Stop(CommandContext &Context) { GpuTimeManager::StopTimer(Context, m_TimerIndex); }

    float GetTime(void) { return GpuTimeManager::GetTime(m_TimerIndex); }

    uint32_t GetTimerIndex(void) { return m_TimerIndex; }

private:
    uint32_t m_TimerIndex;
};

// A tree that records timing
// times of children are included in parent's time
class NestedTimingTree
{
public:
    NestedTimingTree(const std::string &name, NestedTimingTree *parent = nullptr)
        : m_Name(name), m_Parent(parent), m_IsExpanded(false)
    {
    }

    NestedTimingTree *GetChild(const std::string &name)
    {
        auto iter = m_LUT.find(name);
        if (iter != m_LUT.end())
            return iter->second;

        NestedTimingTree *node = new NestedTimingTree(name, this);
        m_Children.push_back(node);
        m_LUT[name] = node;
        return node;
    }

    NestedTimingTree *NextScope(void)
    {
        // if current scope is expanded, next scope is the first child,
        // otherwise is its next sibling
        if (m_IsExpanded && m_Children.size() > 0)
            return m_Children[0];

        return m_Parent->NextChild(this);
    }

    NestedTimingTree *PrevScope(void)
    {
        // find the prev sibling, show sibling's last child
        NestedTimingTree *prev = m_Parent->PrevChild(this);
        return prev == m_Parent ? prev : prev->LastChild();
    }

    NestedTimingTree *FirstChild(void) { return m_Children.size() == 0 ? nullptr : m_Children[0]; }

    NestedTimingTree *LastChild(void)
    {
        if (!m_IsExpanded || m_Children.size() == 0)
            return this;

        return m_Children.back()->LastChild();
    }

    NestedTimingTree *NextChild(NestedTimingTree *curChild)
    {
        ASSERT(curChild->m_Parent == this);

        for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
        {
            if (*iter == curChild)
            {
                auto nextChild = iter;
                ++nextChild;
                if (nextChild != m_Children.end())
                    return *nextChild;
            }
        }
        // no children. find next sibling's children
        if (m_Parent != nullptr)
            return m_Parent->NextChild(this);
        else
            return &sm_RootScope;
    }

    NestedTimingTree *PrevChild(NestedTimingTree *curChild)
    {
        ASSERT(curChild->m_Parent == this);

        // curChild is the first child. find prev sibling's last child
        if (*m_Children.begin() == curChild)
        {
            if (this == &sm_RootScope)
                return sm_RootScope.LastChild();
            else
                return this;
        }

        for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
        {
            if (*iter == curChild)
            {
                auto prevChild = iter;
                --prevChild;
                return *prevChild;
            }
        }

        ERROR("All attempts to find a previous timing sample failed");
        return nullptr;
    }

    void StartTiming(CommandContext *Context)
    {
        m_StartTick = SystemTime::GetCurrentTick();
        if (Context == nullptr)
            return;

        m_GpuTimer.Start(*Context);

        // Context->PIXBeginEvent(m_Name.c_str());
    }

    void StopTiming(CommandContext *Context)
    {
        m_EndTick = SystemTime::GetCurrentTick();
        if (Context == nullptr)
            return;

        m_GpuTimer.Stop(*Context);

        // Context->PIXEndEvent();
    }

private:
    std::string m_Name;
    NestedTimingTree *m_Parent;
    std::vector<NestedTimingTree *> m_Children;
    bool m_IsExpanded;
    std::unordered_map<std::string, NestedTimingTree *> m_LUT;
    int64_t m_StartTick;
    int64_t m_EndTick;
    GpuTimer m_GpuTimer;

    static NestedTimingTree sm_RootScope;
};
