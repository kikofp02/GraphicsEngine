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

uniform sampler2D uTexture;
uniform int uDisplayMode;

in vec2 vTexCoord;
out vec4 oColor;

vec3 normalToColor(vec3 n) {
    return (n + 1.0) * 0.5; // Map [-1,1] to [0,1]
}

vec3 positionToColor(vec3 p) {
    return fract(p * 0.1); // Visualize world position
}

void main() {
    switch(uDisplayMode) {
        case 0: // Albedo
            oColor = texture(uTexture, vTexCoord);
            break;
            
        case 1: // Normals
            vec3 normal = texture(uTexture, vTexCoord).rgb;
            oColor = vec4(normalToColor(normal), 1.0);
            break;
            
        case 2: // Positions
            vec3 position = texture(uTexture, vTexCoord).rgb;
            oColor = vec4(positionToColor(position), 1.0);
            break;
            
        case 3: // Depth
            float depth = texture(uTexture, vTexCoord).r;
            oColor = vec4(vec3(depth), 1.0);
            break;
    }
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

uniform sampler2D uTexture;

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

layout(location = 0) out vec4 oColor;

vec3 CalculateDirectionalLight(uint index, vec3 normal, vec3 viewDir) {
    // Quad calculation (full-screen effect)
    vec3 lightDir = normalize(-uLight[index].direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Simple specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color * (diff + spec);
}

vec3 CalculatePointLight(uint index, vec3 normal, vec3 viewDir) {
    // Sphere calculation
    vec3 lightPos = uLight[index].position.xyz;
    float range = uLight[index].position.w;
    
    vec3 lightVec = lightPos - vFragPos;
    float distance = length(lightVec);
    if(distance > range) return vec3(0.0);
    
    vec3 lightDir = normalize(lightVec);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Attenuation
    float attenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color.xyz * (diff + spec) * attenuation;
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

///////////////////////////////////////////////////////////////////////
#ifdef GEOMETRY_PASS

#if defined(VERTEX)
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

// Uniforms remain the same
layout(std140, binding=1) uniform TransformBlock {
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

#elif defined(FRAGMENT)

uniform sampler2D uTexture;

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec3 oPosition;

void main() {
    // Albedo with alpha
    oAlbedo = texture(uTexture, vTexCoord);
    
    // World-space normal [-1,1]
    oNormal = normalize(vNormal);
    
    // World-space position
    oPosition = vFragPos;
    
    // Depth handled automatically
}
#endif
#endif

///////////////////////////////////////////////////////////////////////
#ifdef DEFERRED_LIGHTING

#if defined(VERTEX)
// Same as textured quad vertex shader
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT)
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
    // Quad calculation (full-screen effect)
    vec3 lightDir = normalize(-uLight[index].direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Simple specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color * (diff + spec);
}

vec3 CalculatePointLight(uint index, vec3 normal, vec3 fragPos, vec3 viewDir) {
    // Sphere calculation
    vec3 lightPos = uLight[index].position.xyz;
    float range = uLight[index].position.w;
    
    vec3 lightVec = lightPos - fragPos;
    float distance = length(lightVec);
    if(distance > range) return vec3(0.0);
    
    vec3 lightDir = normalize(lightVec);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Attenuation
    float attenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    return uLight[index].color.xyz * (diff + spec) * attenuation;
}

void main()
{
    // Retrieve G-buffer data
    vec3 albedo = texture(gAlbedo, vTexCoord).rgb;
    vec3 normal = normalize(texture(gNormal, vTexCoord).rgb);
    vec3 fragPos = texture(gPosition, vTexCoord).rgb;

    vec3 viewDir = normalize(uCameraPosition - fragPos);

    vec3 result = vec3(0.0);
    for(uint i = 0u; i < uLightCount; i++) {
        if(uLight[i].type == 0u) { // Directional light
            result += CalculateDirectionalLight(i, normal, viewDir);
        } else { // Point light
            result += CalculatePointLight(i, normal, fragPos, viewDir);
        }
    }

    oColor = vec4(result * albedo, 1.);
}
#endif
#endif