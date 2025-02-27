#include "core/application.h"

#include "managers/shader_manager.h"

#include "renderer/deferred/deferred_renderer.h"

#include "render_core/render_graph/render_graph.h"
#include "render_core/render_graph/render_graph_blackboard.h"
#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_utils.h"

#include "render_core/resources/buffers.h"
#include "render_core/resources/camera.h"
#include "render_core/resources/cubemap.h"
#include "render_core/resources/material.h"
#include "render_core/resources/texture.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/presenter.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/sampler_state.h"
#include "render_core/rhi/shader.h"
#include "render_core/rhi/synchronization.h"

#include "render_core/shader/shader_declaration.h"

#include "scene/entity.h"
#include "scene/scene.h"