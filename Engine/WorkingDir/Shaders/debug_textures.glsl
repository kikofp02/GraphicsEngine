///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef DEBUG_TEXTURES

#if defined(VERTEX) ///////////////////////////////////////////////////

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
    return (n + 1.0) * 0.5;
}

vec3 positionToColor(vec3 p) {
    float freq = 0.001; // controls spacing
    float sharpness = 200.0; // higher = more contrast

    vec3 wave = sin(p * freq);
    return smoothstep(-1.0 / sharpness, 1.0 / sharpness, wave);
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

        case 4: // Metallic, roughness, Height, AlphaMask
            vec3 matProp = texture(uTexture, vTexCoord).rgb;
            oColor = vec4(matProp, 1.0);
            break;

        case 5: // LightPass
            oColor = texture(uTexture, vTexCoord);
            break;

        case 6: // Brightness
            vec3 brightness = texture(uTexture, vTexCoord).rgb;
            oColor = vec4(brightness, 1.0);
            break;

        case 7: // Blurr
            vec3 blurr = texture(uTexture, vTexCoord).rgb;
            oColor = vec4(blurr, 1.0);
            break;
    }
}

#endif
#endif