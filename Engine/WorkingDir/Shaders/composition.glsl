///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef COMPOSITION

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

out vec4 FragColor;

// Composition textures
uniform sampler2D tScene;
uniform sampler2D tBloom;

// Bloom parameters
uniform bool bloom_enable;
uniform float bloom_exposure;
uniform float bloom_gamma;

void main()
{
    vec3 sceneColor = texture(tScene, vTexCoord).rgb;
    vec3 bloomColor = texture(tBloom, vTexCoord).rgb;

    vec3 result = sceneColor;

    if(bloom_enable){
        result += bloomColor;
        // tone mapping
        result = vec3(1.0) - exp(-result * bloom_exposure);
        // gamma correction      
        result = pow(result, vec3(1.0 / bloom_gamma));
    }
    
    FragColor = vec4(result, 1.0);
}
#endif
#endif