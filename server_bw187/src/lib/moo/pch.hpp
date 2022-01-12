/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MOO_PCH_HPP
#define MOO_PCH_HPP

#ifdef _WIN32
#ifndef MF_SERVER
#include "cstdmf/pch.hpp"

#include "animating_texture.hpp"
#include "animation.hpp"
#include "animation_channel.hpp"
#include "animation_manager.hpp"
#include "base_texture.hpp"
#include "com_object_wrap.hpp"
#include "cube_render_target.hpp"
#include "device_callback.hpp"
#include "directional_light.hpp"
#include "discrete_animation_channel.hpp"
#include "dynamic_index_buffer.hpp"
#include "dynamic_vertex_buffer.hpp"
#include "interpolated_animation_channel.hpp"
#include "light_container.hpp"
#include "managed_texture.hpp"
#include "material.hpp"
#include "effect_material.hpp"
#include "moo_dx.hpp"
#include "moo_math.hpp"
#include "node.hpp"
#include "omni_light.hpp"
#include "primitive.hpp"
#include "primitive_manager.hpp"
#include "render_context.hpp"
#include "render_target.hpp"
#include "shader_manager.hpp"
#include "shader_set.hpp"
#include "software_skinner.hpp"
#include "spot_light.hpp"
#include "texture_manager.hpp"
#include "texturestage.hpp"
#include "vertex_declaration.hpp"
#include "vertex_formats.hpp"
#include "vertices.hpp"
#include "vertices_manager.hpp"
#include "visual.hpp"
#include "visual_manager.hpp"
#endif // MF_SERVER
#endif // _WIN32

#endif // MOO_PCH_HPP
