#version 460
#extension GL_EXT_ray_tracing : enable

#include "ray.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main()
{
    if (payload.depth == 0)
    {
        vec3 clearValue = vec3(1.0);
        payload.hitValue = clearValue * 0.8;
    }
    else
    {
        payload.hitValue = vec3(0.01); // No contribution from environment
    }

    payload.depth = 100; // Ending trace
}
