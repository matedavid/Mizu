#include "shader_reflection.h"

#include "base/debug/logging.h"

namespace Mizu
{

SlangReflection::SlangReflection(std::string_view data)
{
    const nlohmann::json json_data = nlohmann::json::parse(data);

    parse_entry_point(json_data["entryPoints"][0]);
    parse_resources(json_data["parameters"]);
}

void SlangReflection::parse_entry_point(const nlohmann::json& data)
{
    const auto shader_stage_string_to_shader_type = [](std::string_view stage) -> ShaderType {
        if (stage == "vertex")
            return ShaderType::Vertex;
        else if (stage == "fragment")
            return ShaderType::Fragment;
        else if (stage == "compute")
            return ShaderType::Compute;

        MIZU_UNREACHABLE("Shader stage not implemented");
    };

    const auto is_builtin_input = [](const nlohmann::json& input) -> bool {
        return !input.contains("binding") || input["binding"]["kind"].get<std::string_view>() != "varyingInput";
    };

    m_entry_point = data["name"].get<std::string>();
    m_shader_type = shader_stage_string_to_shader_type(data["stage"]);

    // Parse inputs
    if (data.contains("parameters"))
    {
        const nlohmann::json& json_inputs = data["parameters"];

        for (const nlohmann::json& json_input : json_inputs)
        {
            if (is_builtin_input(json_input))
                continue;

            ShaderInputOutput value{};

            if (json_input["type"]["kind"].get<std::string_view>() == "struct")
            {
                uint32_t location = 0;
                for (const nlohmann::json& json_input_field : json_input["type"]["fields"])
                {
                    ShaderInputOutput io = parse_entry_point_input_output(json_input_field);
                    io.location = location;

                    location += 1;

                    m_inputs.push_back(io);
                }
            }
            else
            {
                m_inputs.push_back(parse_entry_point_input_output(json_input));
            }
        }
    }

    for (const ShaderInputOutput& input : m_inputs)
    {
        MIZU_LOG_INFO(
            "Input: {} {} - {} {}",
            input.primitive.name,
            input.primitive.type.as_string(),
            input.semantic_name,
            input.location);
    }

    // Parse outputs
    if (data.contains("result"))
    {
        const nlohmann::json& json_output = data["result"];

        if (json_output["type"]["kind"].get<std::string_view>() == "struct")
        {
            uint32_t location = 0;
            for (const nlohmann::json& json_output_field : json_output["type"]["fields"])
            {
                ShaderInputOutput io = parse_entry_point_input_output(json_output_field);
                io.location = location;

                location += 1;

                m_outputs.push_back(io);
            }
        }
        else
        {
            m_outputs.push_back(parse_entry_point_input_output(json_output));
        }
    }

    for (const ShaderInputOutput& output : m_outputs)
    {
        MIZU_LOG_INFO(
            "Output: {} {} - {} {}",
            output.primitive.name,
            output.primitive.type.as_string(),
            output.semantic_name,
            output.location);
    }
}

ShaderInputOutput SlangReflection::parse_entry_point_input_output(const nlohmann::json& data) const
{
    ShaderInputOutput value{};
    value.primitive = parse_primitive(data);
    value.semantic_name = data.value("semanticName", "");

    if (data.contains("semanticIndex"))
    {
        value.semantic_name += std::to_string(data["semanticIndex"].get<uint32_t>());
    }

    if (data.contains("binding"))
    {
        value.location = data["binding"].value("index", 0u);
    }

    return value;
}

static std::string_view shader_resource_to_name(ShaderResourceT type)
{
    if (std::holds_alternative<ShaderResourceTexture>(type))
    {
        const ShaderResourceTexture& texture = std::get<ShaderResourceTexture>(type);
        if (texture.access == ShaderResourceAccessType::ReadOnly)
        {
            return "Texture";
        }
        else
        {
            return "RWTexture";
        }
    }
    else if (std::holds_alternative<ShaderResourceConstantBuffer>(type))
    {
        return "ConstantBuffer";
    }
    else if (std::holds_alternative<ShaderResourceStructuredBuffer>(type))
    {
        const ShaderResourceStructuredBuffer& buffer = std::get<ShaderResourceStructuredBuffer>(type);
        if (buffer.access == ShaderResourceAccessType::ReadOnly)
        {
            return "StructuredBuffer";
        }
        else
        {
            return "RWStructuredBuffer";
        }
    }
    else if (std::holds_alternative<ShaderResourceSamplerState>(type))
    {
        return "SamplerState";
    }

    MIZU_UNREACHABLE("Invalid ShaderResourceT");
}

void SlangReflection::parse_resources(const nlohmann::json& data)
{
    const auto parse_binding_info = [](const nlohmann::json& json_binding) -> ShaderBindingInfo {
        ShaderBindingInfo info{};
        info.binding = json_binding.value("index", 0);
        info.set = json_binding.value("space", 0);

        return info;
    };

    for (const nlohmann::json& json_resource : data)
    {
        ShaderResource resource{};
        resource.name = json_resource.value("name", "");
        resource.binding_info = parse_binding_info(json_resource["binding"]);

        const nlohmann::json json_resource_type = json_resource["type"];
        const std::string_view type = json_resource_type["kind"].get<std::string_view>();
        const std::string base_shape = json_resource_type.value("baseShape", "");

        if (type == "samplerState")
        {
            resource.value = ShaderResourceSamplerState{};
        }
        else if (type == "constantBuffer")
        {
            ShaderResourceConstantBuffer constant_buffer{};

            const nlohmann::json json_struct_fields = json_resource_type["elementType"]["fields"];
            for (const nlohmann::json& json_field : json_struct_fields)
            {
                constant_buffer.members.push_back(parse_primitive(json_field));
            }

            resource.value = constant_buffer;
        }
        else if (type == "resource" && base_shape == "structuredBuffer")
        {
            ShaderResourceStructuredBuffer structured_buffer{};
            structured_buffer.access = ShaderResourceAccessType::ReadOnly;

            if (json_resource_type.contains("access")
                && json_resource_type["access"].get<std::string_view>() == "readWrite")
            {
                structured_buffer.access = ShaderResourceAccessType::ReadWrite;
            }
            else
            {
                structured_buffer.access = ShaderResourceAccessType::ReadOnly;
            }

            resource.value = structured_buffer;
        }
        else if (type == "resource" && base_shape == "texture2D")
        {
            ShaderResourceTexture texture{};
            texture.access = ShaderResourceAccessType::ReadOnly;

            if (json_resource_type.contains("access")
                && json_resource_type["access"].get<std::string_view>() == "readWrite")
            {
                texture.access = ShaderResourceAccessType::ReadWrite;
            }
            else
            {
                texture.access = ShaderResourceAccessType::ReadOnly;
            }

            resource.value = texture;
        }
        else
        {
            MIZU_UNREACHABLE("Invalid resource type");
        }

        m_resources.push_back(resource);
    }

    for (const ShaderResource& resource : m_resources)
    {
        MIZU_LOG_INFO("Resource: {} {}", resource.name, shader_resource_to_name(resource.value));

        if (std::holds_alternative<ShaderResourceConstantBuffer>(resource.value))
        {
            const ShaderResourceConstantBuffer& value = std::get<ShaderResourceConstantBuffer>(resource.value);
            for (const ShaderPrimitive& member : value.members)
            {
                MIZU_LOG_INFO("    {}: {}", member.name, member.type.as_string());
            }
        }
    }
}

ShaderPrimitive SlangReflection::parse_primitive(const nlohmann::json& data) const
{
    ShaderPrimitive primitive{};
    primitive.name = data.value("name", "");
    primitive.type = parse_primitive_type(data["type"]);

    return primitive;
}

ShaderPrimitiveType SlangReflection::parse_primitive_type(const nlohmann::json& data) const
{
    if (data["kind"].get<std::string_view>() == "scalar")
    {
        return parse_primitive_type_scalar(data);
    }
    else
    {
        return parse_primitive_type_composed(data);
    }
}

ShaderPrimitiveType SlangReflection::parse_primitive_type_scalar(const nlohmann::json& data) const
{
    const std::string_view& type_str = data["scalarType"];

    if (type_str == "float32")
        return ShaderPrimitiveType::Float;
    else if (type_str == "uint32")
        return ShaderPrimitiveType::UInt;
    else if (type_str == "uint64")
        return ShaderPrimitiveType::ULong;
}

ShaderPrimitiveType SlangReflection::parse_primitive_type_composed(const nlohmann::json& data) const
{
    const std::string_view& kind = data["kind"];
    const ShaderPrimitiveType scalar_type = parse_primitive_type_scalar(data["elementType"]);

    if (kind == "vector")
    {
        const uint32_t num_elements = data["elementCount"].get<uint32_t>();

        // clang-format off
        if (scalar_type == ShaderPrimitiveType::Float)
        {
            if (num_elements == 2) return ShaderPrimitiveType::Float2;
            if (num_elements == 3) return ShaderPrimitiveType::Float3;
            if (num_elements == 4) return ShaderPrimitiveType::Float4;
        }
        else if (scalar_type == ShaderPrimitiveType::UInt)
        {
            if (num_elements == 2) return ShaderPrimitiveType::UInt2;
            if (num_elements == 3) return ShaderPrimitiveType::UInt3;
            if (num_elements == 4) return ShaderPrimitiveType::UInt4;
        }
        // clang-format on
    }
    else if (kind == "matrix")
    {
        const uint32_t num_rows = data["rowCount"];
        const uint32_t num_cols = data["columnCount"];

        // clang-format off
        if (scalar_type == ShaderPrimitiveType::Float)
        {
            if (num_rows == 3 && num_cols == 3) return ShaderPrimitiveType::Float3x3;
            if (num_rows == 4 && num_cols == 4) return ShaderPrimitiveType::Float4x4;
        }
        // clang-format on
    }

    MIZU_UNREACHABLE("Invalid composed primitive type");
}

} // namespace Mizu