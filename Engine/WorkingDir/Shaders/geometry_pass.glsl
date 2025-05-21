///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef GEOMETRY_PASS

struct Mat_Prop {
    vec4 color;
    bool use_text;
    bool prop_enabled;
};

struct Material {
    Mat_Prop diffuse;
    Mat_Prop metallic;
    Mat_Prop roughness;
    Mat_Prop normal;
    Mat_Prop height;
    Mat_Prop alphaMask;
};

struct Mat_Textures{
    sampler2D diffuse;
    sampler2D metallic;
    sampler2D roughness;
    sampler2D normal;
    sampler2D height;
    sampler2D alphaMask;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;

layout(std140, binding=1) uniform TransformBlock {
    mat4 uModelMatrix;
    mat4 uViewProjectionMatrix;
};

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;
out mat3 vTBN;

void main()
{
    vTexCoord = aTexCoord;
    vNormal = mat3(transpose(inverse(uModelMatrix))) * aNormal;
    vFragPos = vec3(uModelMatrix * vec4(aPosition, 1.0));

    // calculate TBN matrix
    vec3 T = normalize(vec3(uModelMatrix * vec4(aTangent, 0.0)));
    vec3 B = normalize(vec3(uModelMatrix * vec4(aBitangent, 0.0)));
    vec3 N = normalize(vec3(uModelMatrix * vec4(aNormal, 0.0)));
    vTBN = mat3(T, B, N);

    gl_Position = uViewProjectionMatrix * uModelMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;
in mat3 vTBN;

struct Light {      // position.w = range   ||  color.a = intensity
    bool enable;
    uint type;
    vec3 direction;
    vec4 color;
    vec4 position;
};

layout(std140, binding = 0) uniform GlobalParams {
    vec3            uCameraPosition;
    uint            uLightCount;
    Light           uLight[16];
};

uniform Material material;
uniform Mat_Textures mat_textures;

// Parallax mapping settings
uniform float parallaxScale;
uniform float numLayers;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec3 oPosition;
layout(location = 3) out vec4 oMatProps;

// Parallax Occlusion Mapping
///////////////////////////////////////////////////////////////////////
vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir) 
{
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    vec2 P = viewDir.xy * parallaxScale; 
    vec2 deltaTexCoords = P / numLayers;
    
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(mat_textures.height, currentTexCoords).r;
    
    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(mat_textures.height, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(mat_textures.height, prevTexCoords).r - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
    
    return finalTexCoords;
}

void main() {

    vec2 texCoords = vTexCoord;
    if(material.height.prop_enabled && material.height.use_text) {
        vec3 viewDir = normalize(uCameraPosition - vFragPos);
        viewDir = normalize(transpose(vTBN) * viewDir);
        
        texCoords = ParallaxMapping(vTexCoord, viewDir);
        
        // Discard fragments that are sampled outside the texture
        if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
            discard;
    }

    // Albedo
    vec4 texColor = material.diffuse.use_text ? texture(mat_textures.diffuse, texCoords) : material.diffuse.color;
    oAlbedo = texColor;
    
    // Normal
    vec3 normal = material.normal.use_text ? (texture(mat_textures.normal, texCoords).xyz * 2.0 - 1.0) : (material.normal.color.xyz * 2.0 - 1.0);
    
    vec3 norm = normalize(vNormal);
    if(material.normal.prop_enabled){norm = normalize(vTBN * normal);}

    oNormal = norm;
    
    // Position
    oPosition = vFragPos;
    
    // Material Properties
    float metallic = material.metallic.use_text ? texture(mat_textures.metallic, texCoords).r : material.metallic.color.r;

    float roughness = 0.0f;
    if(material.roughness.prop_enabled){
        roughness = material.roughness.use_text ? texture(mat_textures.roughness, texCoords).r : material.roughness.color.r;
    }

    float height = 0.0f;
    if(material.height.prop_enabled){
        height = material.height.use_text ? texture(mat_textures.height, texCoords).r : material.height.color.r;
    }

    if (material.alphaMask.prop_enabled) {
        float alphaMask = material.alphaMask.use_text ? texture(mat_textures.alphaMask, texCoords).a : material.alphaMask.color.a;
        oAlbedo.a = alphaMask;
    }

    oMatProps = vec4(metallic, roughness, height, 1.0);
}
#endif
#endif