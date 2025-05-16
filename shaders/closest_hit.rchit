#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "bindless.glsl"
#include "ray.glsl"
#include "sampling.glsl"

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 texCoord;
};

struct Triangle
{
	Vertex vertices[3];
	vec3 position;
	vec3 normal;
	vec2 texCoord;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer Indices { uint indices[]; };

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT vec2 attribs;

Triangle UnpackGeometry(GeometryNode geometryNode)
{
    Vertices vertices = Vertices(geometryNode.vertexBufferDeviceAddress);
    Indices indices = Indices(geometryNode.indexBufferDeviceAddress);

    Triangle triangle;
    const uint indexOffset = gl_PrimitiveID * 3;

    for (uint i = 0; i < 3; ++i)
    {
        const uint offset = indices.indices[indexOffset + i];
        triangle.vertices[i] = vertices.vertices[offset];
    }

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    triangle.position = triangle.vertices[0].position * barycentricCoords.x + triangle.vertices[1].position * barycentricCoords.y + triangle.vertices[2].position * barycentricCoords.z;
    triangle.normal = triangle.vertices[0].normal * barycentricCoords.x + triangle.vertices[1].normal * barycentricCoords.y + triangle.vertices[2].normal * barycentricCoords.z;
    triangle.texCoord = triangle.vertices[0].texCoord * barycentricCoords.x + triangle.vertices[1].texCoord * barycentricCoords.y + triangle.vertices[2].texCoord * barycentricCoords.z;

    return triangle;
}

void main()
{
    BLASInstance blasInstance = blasInstances[gl_InstanceCustomIndexEXT];
    GeometryNode geometryNode = geometryNodes[blasInstance.firstGeometryIndex + gl_GeometryIndexEXT];
    Material material = materials[nonuniformEXT(geometryNode.materialIndex)];
    Triangle triangle = UnpackGeometry(geometryNode);

    const vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(triangle.position, 1.0));
    const vec3 worldNormal = normalize(vec3(triangle.normal * gl_WorldToObjectEXT));

    // Pick a random direction from here and keep going.
    vec3 tangent, bitangent;
    CreateCoordinateSystem(worldNormal, tangent, bitangent);
    vec3 rayOrigin = worldPosition;
    vec3 rayDirection = SamplingHemisphere(payload.seed, tangent, bitangent, worldNormal);

    const float cosTheta = dot(rayDirection, worldNormal);
    // Probability density function of SamplingHemisphere choosing this rayDirection
    const float directionProbability = cosTheta / PI;

    vec4 albedo = material.albedoFactor;
    if (material.useAlbedoMap)
    {
        albedo *= pow(texture(textures[nonuniformEXT(material.albedoMapIndex)], triangle.texCoord), vec4(2.2));
    }
    vec3 BRDF = albedo.xyz / PI;

    payload.rayOrigin = rayOrigin;
    payload.rayDirection = rayDirection;
    payload.hitValue = material.emissiveFactor;
    payload.weight = BRDF * cosTheta / directionProbability;
}
