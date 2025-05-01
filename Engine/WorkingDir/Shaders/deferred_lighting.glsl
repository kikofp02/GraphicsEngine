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

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gPosition;

struct Light {
    uint type;
    vec3 color;
    vec3 direction;
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


vec3 CalculateDirectionalLight(uint index, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-uLight[index].direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color * (diff + spec);
}

vec3 CalculatePointLight(uint index, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightPos = uLight[index].position.xyz;
    float range = uLight[index].position.w;
    
    vec3 lightVec = lightPos - fragPos;
    float distance = length(lightVec);
    if(distance > range) return vec3(0.0);
    
    vec3 lightDir = normalize(lightVec);
    float diff = max(dot(normal, lightDir), 0.0);

    float attenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color.xyz * (diff + spec) * attenuation;
}

void main()
{
    // G-buffer data
    vec3 albedo = texture(gAlbedo, vTexCoord).rgb;
    vec3 normal = normalize(texture(gNormal, vTexCoord).rgb);
    vec3 fragPos = texture(gPosition, vTexCoord).rgb;

    vec3 viewDir = normalize(uCameraPosition - fragPos);

    vec3 result = vec3(0.0);
    for(uint i = 0u; i < uLightCount; i++) {
        if(uLight[i].type == 0u) {
            result += CalculateDirectionalLight(i, normal, viewDir);
        } else {
            result += CalculatePointLight(i, normal, fragPos, viewDir);
        }
    }

    oColor = vec4(result * albedo, 1.);
}
#endif
#endif