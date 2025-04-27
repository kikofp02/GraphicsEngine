//shaders.glsl
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef TEXTURED_GEOMETRY

#if defined(VERTEX) ///////////////////////////////////////////////////

// TODO: Write your vertex shader here
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = aTexCoord;
	gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

// TODO: Write your fragment shader here
in vec2 vTexCoord;

uniform sampler2D uTexture;

layout(location = 0) out vec4 oColor;

void main()
{
	oColor = texture(uTexture, vTexCoord);
}

#endif
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef TEXTURED_MESH

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
//layout(location=3) in vec3 aTangent;
//layout(location=4) in vec3 aBitangent;

struct Light {
    uint type;
    vec3 color;
    vec3 direction;
    vec3 position;
};

layout(std140, binding = 0) uniform GlobalParams {
    vec3            uCameraPosition;
    uint            uLightCount;
    Light           uLight[16];
};

layout(std140, binding = 1) uniform TransformBlock {
    mat4 uWorldMatrix;
    mat4 uWorldViewProjectionMatrix;
};

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;

void main()
{
	vTexCoord = aTexCoord;
    vNormal = mat3(transpose(inverse(uWorldMatrix))) * aNormal;
    vFragPos = vec3(uWorldMatrix * vec4(aPosition, 1.0));

    gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;

uniform sampler2D uTexture;

struct Light {
    uint type;
    vec3 color;
    vec3 direction;
    vec3 position;
};

layout(std140, binding = 0) uniform GlobalParams {
    vec3            uCameraPosition;
    uint            uLightCount;
    Light           uLight[16];
};

layout(location = 0) out vec4 oColor;

vec3 CalculateDirectionalLight(uint index, vec3 normal, vec3 viewDir) {
    // Light direction
    vec3 lightDir = normalize(-uLight[index].direction);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = uLight[index].color * diff;

    return diffuse;
}

// Point light calculation
vec3 CalculatePointLight(uint index, vec3 normal, vec3 viewDir) {
    // Light direction and distance
    vec3 lightVec = uLight[index].position - vFragPos;
    float distance = length(lightVec);
    vec3 lightDir = lightVec / distance;
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = uLight[index].color * diff;

    // Attenuation (simple quadratic falloff)
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
    
    return diffuse * attenuation;
}

void main()
{
    // Basic surface properties
    vec3 norm = normalize(vNormal);
    vec3 viewDir = normalize(uCameraPosition - vFragPos);
    vec3 result = vec3(0.0);

    // Calculate lighting contributions
    for(uint i = 0u; i < uLightCount; i++) {
        if(uLight[i].type == 0u) { // Directional light
            result += CalculateDirectionalLight(i, norm, viewDir);
        } else { // Point light
            result += CalculatePointLight(i, norm, viewDir);
        }
    }

    // Combine with texture
    vec4 texColor = texture(uTexture, vTexCoord);
    oColor = vec4(result * texColor.rgb, texColor.a);
    //oColor = vec4(norm * 0.5 + 0.5, 1.0);
}

#endif
#endif