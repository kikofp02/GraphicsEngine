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

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;

layout(std140, binding = 1) uniform TransformBlock {
    mat4 uWorldMatrix;
    mat4 uWorldViewProjectionMatrix;
};

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
uniform vec3 uViewPos;

layout(location = 0) out vec4 oColor;

void main()
{
    // Combine with texture
    vec4 texColor = texture(uTexture, vTexCoord);
    oColor = vec4(texColor.rgb, texColor.a);
    //oColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
}

#endif
#endif