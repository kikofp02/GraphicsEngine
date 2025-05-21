///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef BLOOM_PASS

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

uniform sampler2D image;

uniform bool horizontal;
uniform float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

void main()
{
    vec2 tex_offset = 1.0 / textureSize(image, 0);
    vec3 result = texture(image, vTexCoord).rgb * weight[0];

    if(horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, vTexCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(image, vTexCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, vTexCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(image, vTexCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
#endif
#endif