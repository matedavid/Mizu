#include "shader_reflection.h"

#include "base/debug/logging.h"

namespace Mizu
{

SlangReflection::SlangReflection(std::string_view data)
{
    const nlohmann::json json_data = nlohmann::json::parse(data);

    parse_parameters(json_data["parameters"]);
    parse_push_constants(json_data["push_constants"]);
    parse_inputs_outputs(json_data["inputs"], m_inputs);
    parse_inputs_outputs(json_data["outputs"], m_outputs);
}

void SlangReflection::parse_parameters(const nlohmann::json& json_parameters)
{
    for (const nlohmann::json& json_parameter : json_parameters)
    {
        ShaderResource& resource = m_parameters.emplace_back();
        resource.name = json_parameter["name"].get<std::string>();
        resource.binding_info = ShaderBindingInfo{
            .set = json_parameter["binding_info"]["set"].get<uint32_t>(),
            .binding = json_parameter["binding_info"]["binding"].get<uint32_t>(),
        };

        const std::string_view resource_type = json_parameter["resource_type"].get<std::string_view>();
        if (resource_type == "texture")
        {
            ShaderResourceTexture texture{};
            texture.access = static_cast<ShaderResourceAccessType>(json_parameter["access_type"].get<uint32_t>());

            resource.value = texture;
        }
        else if (resource_type == "structured_buffer")
        {
            ShaderResourceStructuredBuffer structured_buffer{};
            structured_buffer.access =
                static_cast<ShaderResourceAccessType>(json_parameter["access_type"].get<uint32_t>());

            resource.value = structured_buffer;
        }
        else if (resource_type == "constant_buffer")
        {
            ShaderResourceConstantBuffer constant_buffer{};
            constant_buffer.total_size = json_parameter["total_size"].get<size_t>();

            for (const nlohmann::json& json_member : json_parameter["members"])
            {
                const ShaderPrimitive member = parse_primitive(json_member);
                constant_buffer.members.push_back(member);
            }

            resource.value = constant_buffer;
        }
        else if (resource_type == "sampler_state")
        {
            resource.value = ShaderResourceSamplerState{};
        }
        else
        {
            MIZU_UNREACHABLE("Invalid resource_type or not implemented");
        }
    }
}

void SlangReflection::parse_push_constants(const nlohmann::json& json_push_constants)
{
    for (const nlohmann::json& json_push_constant : json_push_constants)
    {
        ShaderPushConstant& push_constant = m_constants.emplace_back();
        push_constant.name = json_push_constant["name"].get<std::string>();
        push_constant.binding_info = ShaderBindingInfo{
            .set = json_push_constant["binding_info"]["set"].get<uint32_t>(),
            .binding = json_push_constant["binding_info"]["binding"].get<uint32_t>(),
        };
        push_constant.size = json_push_constant["size"].get<size_t>();
    }
}

void SlangReflection::parse_inputs_outputs(
    const nlohmann::json& json_inputs_outputs,
    std::vector<ShaderInputOutput>& out_vector) const
{
    for (const nlohmann::json& json_input_output : json_inputs_outputs)
    {
        ShaderInputOutput& io = out_vector.emplace_back();
        io.location = json_input_output["location"].get<uint32_t>();
        io.semantic_name = json_input_output["semantic_name"].get<std::string>();
        io.primitive = parse_primitive(json_input_output["primitive"]);
    }
}

ShaderPrimitive SlangReflection::parse_primitive(const nlohmann::json& json_primitive) const
{
    ShaderPrimitive primitive{};
    primitive.name = json_primitive["name"].get<std::string>();
    primitive.type = static_cast<ShaderPrimitiveType::Type>(json_primitive["type"].get<uint32_t>());

    return primitive;
}

} // namespace Mizu