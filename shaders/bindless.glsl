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
    uint materialIndex;
};
layout (std140, set = 0, binding = 2) uniform GeometryNodes
{
    GeometryNode geometryNodes[1024];
};
