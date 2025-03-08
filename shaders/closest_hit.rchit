#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "bindless.glsl"

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texCoord;
};

struct Triangle
{
	Vertex vertices[3];
	vec3 normal;
	vec2 texCoord;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { Vertex vertices[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Indices { uint indices[]; };

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

void main()
{
    GeometryNode geometryNode = geometryNodes[gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];

    Vertices vertices = Vertices(geometryNode.vertexBufferDeviceAddress);
    Indices indices = Indices(geometryNode.indexBufferDeviceAddress);

    Triangle triangle;
    const uint indexOffset = gl_PrimitiveID * 3;

    for (uint i = 0; i < 3; i++)
    {
    	const uint vertexOffset = indices.indices[indexOffset + i];
    	triangle.vertices[i] = vertices.vertices[vertexOffset];
    }

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    triangle.normal = triangle.vertices[0].normal * barycentricCoords.x + triangle.vertices[1].normal * barycentricCoords.y + triangle.vertices[2].normal * barycentricCoords.z;
    triangle.texCoord = triangle.vertices[0].texCoord * barycentricCoords.x + triangle.vertices[1].texCoord * barycentricCoords.y + triangle.vertices[2].texCoord * barycentricCoords.z;

    vec4 albedo = vec4(1.0);
    if (material.useAlbedoMap)
    {
        // TODO: Textures are not correctly sampled somehow and it crashes the program
        albedo = pow(texture(textures[nonuniformEXT(material.albedoMapIndex)], triangle.texCoord), vec4(2.2));
    }
    albedo *= material.albedoFactor;

    hitValue = pow(albedo.rgb, vec3(1.0 / 2.2));
}
