///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef POST_PROCESS

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

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec3 oBloom;

uniform sampler2D deferredImage;

// Bloom parameters
uniform sampler2D blurImage;
uniform bool bloom;
uniform bool horizontal;
uniform float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
uniform float exposure;
uniform float gamma;

void main()
{
    vec3 scene = texture(deferredImage, vTexCoord).rgb;

    if (bloom)
    {
        // Blur
        vec2 tex_offset = 1.0 / textureSize(blurImage, 0); // gets size of single texel
        vec3 result = texture(blurImage, vTexCoord).rgb;

        result = result * weight[0];
        if(horizontal)
        {
            for(int i = 1; i < 5; ++i)
            {
               result += texture(blurImage, vTexCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
               result += texture(blurImage, vTexCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            }
        }
        else
        {
            for(int i = 1; i < 5; ++i)
            {
                result += texture(blurImage, vTexCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                result += texture(blurImage, vTexCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            }
        }

        scene += result;
    }
    
    /* HDR */

    // Tone Mapping
    scene = vec3(1.0) - exp(-scene * exposure);

    // Gamma Correction   
    scene = pow(scene, vec3(1.0 / gamma));


    FragColor = vec4(scene, 1.0);
}
#endif
#endif