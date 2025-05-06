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

uniform Material material;
uniform Mat_Textures mat_textures;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec3 oPosition;
layout(location = 3) out vec4 oMatProps;

void main() {
    // Albedo
    vec4 texColor = material.diffuse.use_text ? texture(mat_textures.diffuse, vTexCoord) : material.diffuse.color;
    oAlbedo = texColor;
    
    // Normal
    vec3 normal = material.normal.use_text ? (texture(mat_textures.normal, vTexCoord).xyz * 2.0 - 1.0) : (material.normal.color.xyz * 2.0 - 1.0);
    
    vec3 norm = normalize(vNormal);
    if(material.normal.prop_enabled){norm = normalize(vTBN * normal);}

    oNormal = norm;
    
    // Position
    oPosition = vFragPos;
    
    // Material Properties
    float metallic = material.metallic.use_text ? texture(mat_textures.metallic, vTexCoord).r : material.metallic.color.r;

    float roughness = 0.0f;
    if(material.roughness.prop_enabled){
        roughness = material.roughness.use_text ? texture(mat_textures.roughness, vTexCoord).r : material.roughness.color.r;
    }

    float height = 0.0f;
    if(material.height.prop_enabled){
        height = material.height.use_text ? texture(mat_textures.height, vTexCoord).r : material.height.color.r;
    }

    if (material.alphaMask.prop_enabled) {
        float alphaMask = material.alphaMask.use_text ? texture(mat_textures.alphaMask, vTexCoord).a : material.alphaMask.color.a;
        oAlbedo.a = alphaMask;
    }

    oMatProps = vec4(metallic, roughness, height, 1.0);
}
#endif
#endif