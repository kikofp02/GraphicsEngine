///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef DEFERRED_LIGHTING

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////////

in vec2 vTexCoord;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 BrightColor;

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gPosition;
uniform sampler2D gMatProps;

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

layout(std140, binding = 1) uniform TransformBlock {
    mat4 uModelMatrix;
    mat4 uViewProjectionMatrix;
};

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

void main()
{
    vec4 albedo = texture(gAlbedo, vTexCoord).rgba;
    vec3 normal = normalize(texture(gNormal, vTexCoord).rgb);
    vec3 fragPos = texture(gPosition, vTexCoord).rgb;
    vec4 matProps = texture(gMatProps, vTexCoord);

    float metallic = matProps.r;
    float roughness = matProps.g;
    float height = matProps.b;

    vec3 viewDir = normalize(uCameraPosition - fragPos);

    vec3 result = vec3(0.0);
    for(uint i = 0u; i < uLightCount; i++) {
        if(uLight[i].enable){
            if(uLight[i].type == 0u) {
                result += CalculateDirectionalLight(i, albedo.rgb, normal, metallic, roughness, fragPos, viewDir);
            } else {
                result += CalculatePointLight(i, albedo.rgb, normal, metallic, roughness, fragPos, viewDir);
            }
        }
    }

    oColor = vec4(result, albedo.a);

    // Brightness texture pixel detection
    float brightness = dot(result.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(result.rgb, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
#endif
#endif