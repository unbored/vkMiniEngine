#include "GpuTimeManager.h"
#include "CommandBufferManager.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include <vulkan/vulkan_structs.hpp>

namespace
{
vk::QueryPool sm_QueryPool = nullptr;
uint32_t sm_MaxNumTimers = 0;
uint32_t sm_NumTimers = 1; // first timer is for global use
} // namespace

void GpuTimeManager::Initialize(uint32_t MaxNumTimers)
{
    vk::QueryPoolCreateInfo queryInfo;
    queryInfo.queryType = vk::QueryType::eTimestamp;
    queryInfo.queryCount = MaxNumTimers * 2; // one for begin and one for end
    sm_QueryPool = Graphics::g_Device.createQueryPool(queryInfo);
    sm_MaxNumTimers = MaxNumTimers;
}

void GpuTimeManager::Shutdown()
{
    if (sm_QueryPool)
    {
        Graphics::g_Device.destroyQueryPool(sm_QueryPool);
        sm_QueryPool = nullptr;
    }
}

uint32_t GpuTimeManager::NewTimer(void) { return sm_NumTimers++; }

void GpuTimeManager::StartTimer(CommandContext &Context, uint32_t TimerIdx)
{
    Context.InsertTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, sm_QueryPool, TimerIdx * 2);
}

void GpuTimeManager::StopTimer(CommandContext &Context, uint32_t TimerIdx)
{
    Context.InsertTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, sm_QueryPool, TimerIdx * 2 + 1);
}
