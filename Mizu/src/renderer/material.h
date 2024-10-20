#pragma once

#include <memory>
#include <vector>

#include "renderer/abstraction/resource_group.h"
#include "renderer/shader/material_shader.h"
#include "utility/assert.h"

namespace Mizu {

// Forward declarations
class IShader;

class IMaterial {
  public:
    virtual ~IMaterial() = default;

    [[nodiscard]] virtual std::vector<std::shared_ptr<ResourceGroup>> get_resource_groups() const = 0;
};

template <typename MatShaderT>
class Material : public IMaterial {
    static_assert(std::is_base_of_v<MaterialShader<typename MatShaderT::Parent>, MatShaderT>,
                  "MatShaderT must inherit from MaterialShader");

  public:
    Material() = default;

    bool init(const typename MatShaderT::MaterialParameters& mat_params) {
        const std::shared_ptr<IShader>& shader = MatShaderT::get_shader();

        const std::vector<MaterialParameterInfo> parameters = MatShaderT::MaterialParameters::get_members(mat_params);
        if (parameters.empty()) {
            MIZU_LOG_WARNING("MaterialShader has no Material Properties");
            return false;
        }

        // Get shader properties
        uint32_t max_binding_set = 0;

        std::vector<ShaderProperty> shader_properties;
        shader_properties.reserve(parameters.size());

        for (const MaterialParameterInfo& mat_param : parameters) {
            const auto property = shader->get_property(mat_param.param_name);
            if (!property.has_value()) {
                MIZU_LOG_ERROR("Material Property '{}' not found in shader", mat_param.param_name);
                return false;
            }

            shader_properties.push_back(*property);
            max_binding_set = std::max(max_binding_set, property->binding_info.set);

            // TODO: Should log Warning if all properties don't have the same binding set
        }

        MIZU_ASSERT(shader_properties.size() == parameters.size(),
                    "Shader properties and material properties should have the same size");

        // Create resource groups
        std::vector<std::shared_ptr<ResourceGroup>> set_to_resource_group(max_binding_set + 1, nullptr);

        for (size_t i = 0; i < shader_properties.size(); ++i) {
            const auto& shader_prop = shader_properties[i];
            const auto& mat_prop = parameters[i];

            if (set_to_resource_group[shader_prop.binding_info.set] == nullptr) {
                set_to_resource_group[shader_prop.binding_info.set] = ResourceGroup::create();
            }

            if (std::holds_alternative<std::shared_ptr<Texture2D>>(mat_prop.value)) {
                set_to_resource_group[shader_prop.binding_info.set]->add_resource(
                    mat_prop.param_name, std::get<std::shared_ptr<Texture2D>>(mat_prop.value));
            }
        }

        // Bake resource groups
        for (size_t i = 0; i < set_to_resource_group.size(); ++i) {
            auto& resource_group = set_to_resource_group[i];
            if (resource_group == nullptr)
                continue;

            if (!resource_group->bake(shader, i)) {
                MIZU_LOG_ERROR("Could not bake material resource group with set {}", i);
                return false;
            }

            m_resource_groups.push_back(resource_group);
        }

        return true;
    }

    [[nodiscard]] std::vector<std::shared_ptr<ResourceGroup>> get_resource_groups() const override {
        return m_resource_groups;
    }

  private:
    std::vector<std::shared_ptr<ResourceGroup>> m_resource_groups;
};

} // namespace Mizu
