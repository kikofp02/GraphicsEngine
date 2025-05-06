///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef FORWARD

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

struct Light {      // position.w = range   ||  color.a = intensity
    bool enable;
    uint type;
    vec3 direction;
    vec4 color;
    vec4 position;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec3 aTangent;
layout(location=4) in vec3 aBitangent;

layout(std140, binding = 1) uniform TransformBlock {
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

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;
in mat3 vTBN;

uniform Material material;
uniform Mat_Textures mat_textures;

layout(std140, binding = 0) uniform GlobalParams {
    vec3            uCameraPosition;
    uint            uLightCount;
    Light           uLight[16];
};

layout(location = 0) out vec4 oColor;

const float PI = 3.14159265359;

// PBR Functions
///////////////////////////////////////////////////////////////////////

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Normal Distribution Function (GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// Geometry function (Smith's method)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// PBR Lighting Calculation
///////////////////////////////////////////////////////////////////////

vec3 CalculateLight(vec3 albedo, vec3 normal, float metallic, float roughness,
                    vec3 fragPos, vec3 viewDir, vec3 lightDir, vec3 radiance)
{
    vec3 halfwayDir = normalize(viewDir + lightDir);

    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = FresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 numerator = NDF * G * F;
    float denom = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.001;
    vec3 specular = numerator / denom;

    float NdotL = max(dot(normal, lightDir), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 CalculateDirectionalLight(uint index, vec3 albedo, vec3 normal, float metallic, float roughness, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(-uLight[index].direction);
    vec3 lightColor = uLight[index].color.xyz;
    vec3 radiance = ((lightColor) * uLight[index].color.a);
    return CalculateLight(albedo, normal, metallic, roughness, fragPos, viewDir, lightDir, radiance);
}

vec3 CalculatePointLight(uint index, vec3 albedo, vec3 normal, float metallic, float roughness, vec3 fragPos, vec3 viewDir) {
    vec3 lightPos = uLight[index].position.xyz;
    float range = uLight[index].position.w;
    vec3 lightVec = lightPos - fragPos;
    float distance = length(lightVec);
    if(distance > range) return vec3(0.0);

    float attenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    vec3 lightColor = uLight[index].color.xyz;
    vec3 radiance = ((lightColor) * uLight[index].color.a) * attenuation;
    vec3 lightDir = normalize(lightVec);

    return CalculateLight(albedo, normal, metallic, roughness, fragPos, viewDir, lightDir, radiance);
}

void main() {

    // Albedo
    vec4 texColor = material.diffuse.use_text ? texture(mat_textures.diffuse, vTexCoord) : material.diffuse.color;
    vec3 albedo = texColor.rgb;
    float alpha = texColor.a;

    // Alpha Masking
    if (material.alphaMask.prop_enabled) {
        vec3 maskRGB = material.alphaMask.use_text ? texture(mat_textures.alphaMask, vTexCoord).rgb : material.alphaMask.color.rgb;
        float alphaMask = dot(maskRGB, vec3(0.299, 0.587, 0.114));
        alpha = alphaMask;
    }

    // Metallic
    float metallic = 0.0;
    if (material.metallic.prop_enabled) {
        metallic = material.metallic.use_text ? texture(mat_textures.metallic, vTexCoord).r : material.metallic.color.r;
    }

    // Roughness
    float roughness = 0.0;
    if (material.roughness.prop_enabled) {
        roughness = material.roughness.use_text ? texture(mat_textures.roughness, vTexCoord).r : material.roughness.color.r;
    }

    // Normal Mapping
    vec3 textureNormal = material.normal.use_text ? texture(mat_textures.normal, vTexCoord).xyz * 2.0 - 1.0 : material.normal.color.xyz * 2.0 - 1.0;
    vec3 normal = normalize(vNormal);
    if (material.normal.prop_enabled) {
        normal = normalize(vTBN * textureNormal);
    }

    vec3 viewDir = normalize(uCameraPosition - vFragPos);
    vec3 result = vec3(0.0);

    

    for(uint i = 0u; i < uLightCount; i++) {
        if(uLight[i].enable){
            if(uLight[i].type == 0u) {
                result += CalculateDirectionalLight(i, albedo.rgb, normal, metallic, roughness, vFragPos, viewDir);
            } else {
                result += CalculatePointLight(i, albedo.rgb, normal, metallic, roughness, vFragPos, viewDir);
            }
        }
    }

    oColor = vec4(result, alpha);
}

#endif
#endif