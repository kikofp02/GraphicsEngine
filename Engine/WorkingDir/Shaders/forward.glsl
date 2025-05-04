///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef FORWARD

struct Mat_Prop {
    vec4 color;
    bool use_text;
};

struct Material {
    Mat_Prop diffuse;
    Mat_Prop specular;
    Mat_Prop normal;
    Mat_Prop height;
};

struct Mat_Textures{
    sampler2D diffuse;
    sampler2D specular;
    sampler2D normal;
    sampler2D height;
};

struct Light {
    uint type;
    vec3 color;
    vec3 direction;
    vec4 position;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
//layout(location=3) in vec3 aTangent;
//layout(location=4) in vec3 aBitangent;

layout(std140, binding = 0) uniform GlobalParams {
    vec3            uCameraPosition;
    uint            uLightCount;
    Light           uLight[16];
};

layout(std140, binding = 1) uniform TransformBlock {
    mat4 uModelMatrix;
    mat4 uViewProjectionMatrix;
};

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;

void main()
{
	vTexCoord = aTexCoord;
    vNormal = mat3(transpose(inverse(uModelMatrix))) * aNormal;
    vFragPos = vec3(uModelMatrix * vec4(aPosition, 1.0));

    gl_Position = uViewProjectionMatrix * uModelMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;

uniform Material material;
uniform Mat_Textures mat_textures;

layout(std140, binding = 0) uniform GlobalParams {
    vec3            uCameraPosition;
    uint            uLightCount;
    Light           uLight[16];
};

layout(location = 0) out vec4 oColor;

vec3 CalculateDirectionalLight(uint index, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-uLight[index].direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color * (diff + spec);
}

vec3 CalculatePointLight(uint index, vec3 normal, vec3 viewDir) {
    vec3 lightPos = uLight[index].position.xyz;
    float range = uLight[index].position.w;
    
    vec3 lightVec = lightPos - vFragPos;
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
    vec3 norm = normalize(vNormal);
    vec3 viewDir = normalize(uCameraPosition - vFragPos);
    vec3 result = vec3(0.0);

    for(uint i = 0u; i < uLightCount; i++) {
        if(uLight[i].type == 0u) {
            result += CalculateDirectionalLight(i, norm, viewDir);
        } else {
            result += CalculatePointLight(i, norm, viewDir);
        }
    }

    vec4 texColor = material.diffuse.use_text ? texture(mat_textures.diffuse, vTexCoord) : material.diffuse.color;
    oColor = vec4(result * texColor.rgb, texColor.a);
    //oColor = vec4(norm * 0.5 + 0.5, 1.0);
}

#endif
#endif