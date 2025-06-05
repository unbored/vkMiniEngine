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

#include "DepthBuffer.h"

class GraphicsContext;

class ShadowBuffer : public DepthBuffer
{
public:
    void Create(const std::string &Name, uint32_t width, uint32_t height);

    void BeginRendering(GraphicsContext &context);
    void EndRendering(GraphicsContext &context);

private:
    vk::Viewport m_Viewport;
    vk::Rect2D m_Scissor;
};