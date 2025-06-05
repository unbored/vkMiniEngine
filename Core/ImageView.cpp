#include "ImageView.h"
#include "GraphicsCommon.h"
#include "GraphicsCore.h"
#include "Swapchain.h"
#include "CommandContext.h"

using namespace Graphics;

void ImageView::Destroy()
{
    if (m_ImageView)
    {
        g_Device.destroyImageView(m_ImageView);
        m_ImageView = nullptr;
    }
    if (m_Allocation)
    {
        g_Allocator.destroyImage(m_Image, m_Allocation);
        m_Image = nullptr;
        m_Allocation = nullptr;
    }
}
