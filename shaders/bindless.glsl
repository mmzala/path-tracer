layout (set = 0, binding = 0) uniform sampler2D textures[];

struct Material
{
    vec4 albedoFactor;

    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;

    vec3 emissiveFactor;
    bool useEmissiveMap;

    bool useAlbedoMap;
    bool useMRMap;
    bool useNormalMap;
    bool useOcclusionMap;

    uint albedoMapIndex;
    uint mrMapIndex;
    uint normalMapIndex;
    uint occlusionMapIndex;

    uint emissiveMapIndex;
};
layout (std140, set = 0, binding = 1) uniform Materials
{
    Material materials[1024];
};

struct GeometryNode
{
    uint64_t vertexBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    uint materialIndex;
};
layout (std140, set = 0, binding = 2) buffer GeometryNodes
{
    GeometryNode geometryNodes[];
};
