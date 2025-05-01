///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef GEOMETRY_PASS

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

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

#elif defined(FRAGMENT) ///////////////////////////////////////////////////

uniform sampler2D uTexture;

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec3 oPosition;

void main() {
    // Albedo
    oAlbedo = texture(uTexture, vTexCoord);
    
    // Normal
    oNormal = normalize(vNormal);
    
    // Position
    oPosition = vFragPos;
    
    // Depth
}
#endif
#endif