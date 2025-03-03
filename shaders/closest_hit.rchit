#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "bindless.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;

hitAttributeEXT vec2 attribs;

void main()
{
    Material material = materials[nonuniformEXT(0)];

    // TODO: UVs

    vec4 albedo = vec4(1.0);
    if (material.useAlbedoMap)
    {
        albedo = pow(texture(textures[nonuniformEXT(material.albedoMapIndex)], vec2(0.0)), vec4(2.2));
    }
    albedo *= pow(material.albedoFactor, vec4(2.2));

    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    hitValue = pow(albedo.rgb, vec3(1.0 / 2.2));
}
