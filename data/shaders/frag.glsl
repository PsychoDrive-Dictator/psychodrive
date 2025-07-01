precision mediump float;
precision mediump sampler2D;

uniform vec3 size;
uniform vec3 offset;
uniform vec4 in_color;
uniform int isGrid;
uniform int progress;
uniform sampler2D particleTexture;

const float distedge = 0.5;
const float feather = 0.5;

float boxalpha = 0.2;

in vec4 vertex_pos;
out vec4 color;

float edge(float dist)
{
    if (dist < 0.0001) {
        return boxalpha;
    }
    if (dist < distedge) {
        return 1.0;
    }
    if (dist > distedge + feather)
    {
        return boxalpha;
    }
    float ret = (dist - distedge) / feather; // mapped to 0..1
    ret *= boxalpha - 1.0; // 0..boxalpha-1
    ret = 1.0 + ret; // 1..boxalpha
    return ret;
}

float grid(float dist)
{
    if (dist == 0.0) {
        return boxalpha;
    }
    if (mod(dist,100.0) > 1.0) {
        return boxalpha;
    }
    return 1.0;
}

void main() {
    color = in_color;
    boxalpha = color.a;

    if (isGrid == 1) {
        float edgealpha = boxalpha;
        color = vec4(0.1,0.1,0.1,1.0);
        const float divisor = 765.0 / 8.0;
        if (mod(abs(vertex_pos.x), divisor) < 5.0 ||
            (vertex_pos.y > 0.0 && mod(abs(vertex_pos.y), divisor) < 5.0) ||
            (mod(abs(vertex_pos.z), divisor) < 5.0)) {
            color = vec4(0.3,0.3,0.3,1.0);
        }
        if (abs(vertex_pos.x) < 1.0 || abs(vertex_pos.z) < 1.0) {
            color = vec4(0.9,0.3,0.2,1.0);
        }
        //color.a = edgealpha;
        return;
    }

    if (isGrid == 2) {
        // particles
        vec3 center = offset + size / 2.0;
        vec3 fragPos = vertex_pos.xyz;
        vec3 localPos = (fragPos - center) / size + 0.5; // Normalize to 0-1

        float totalInfluence = 0.0;
        float maxHeat = 0.0;
        float backgroundInfluence = 0.0;
        float backgroundHeat = 0.0;
        const int textureSize = 16;

        vec2 localPos2D = localPos.xy;

        bool isBlockEffect = in_color.r < 0.6 && in_color.g > 0.6 && in_color.b > 0.7;

        for (int i = 0; i < textureSize; i++) {
            for (int j = 0; j < textureSize; j++) {
                vec2 texCoord = vec2(float(j) + 0.5, float(i) + 0.5) / float(textureSize);
                vec4 particleData = texture(particleTexture, texCoord);
                vec2 particlePos2D = particleData.xy;
                float colorVariation = particleData.z;
                float particleFade = particleData.w;

                if (particleFade > 0.01) {
                    float dist = length(localPos2D - particlePos2D);

                    int particleIndex = i * textureSize + j;
                    bool isProjectile = isBlockEffect ? (particleIndex < 128) : (particleIndex < 64);
                    bool isBackground = !isBlockEffect && isProjectile && colorVariation < 0.5;

                    float actualFade = particleFade;
                    float stagger = 0.0;
                    if (isProjectile && particleFade > 0.8) {
                        // fade is encoded as timeFade * 0.8 + stagger * 0.2
                        stagger = (particleFade - 0.8) / 0.2;
                        actualFade = 0.8; // max fade for projectiles
                    } else if (isProjectile) {
                        actualFade = particleFade / 0.8;
                    }

                    float particleRadius = isProjectile ?
                        0.06 :
                        (0.05 + (1.0 - actualFade) * 0.08);

                    if (dist < particleRadius) {
                        float influence = 1.0 - (dist / particleRadius);

                        if (isProjectile) {
                            influence = smoothstep(0.5, 1.0, influence);
                            influence = influence * influence * influence;
                        } else {
                            influence = pow(influence, 2.0 + actualFade * 2.0);
                        }
                        influence *= actualFade;

                        float heat = influence;
                        if (isProjectile) {
                            heat = influence * (1.5 - stagger * 0.8);
                        }

                        float colorShift = (colorVariation - 0.5) * 0.15;
                        heat *= (1.0 + colorShift);

                        if (isBackground) {
                            backgroundInfluence += influence * 0.7;
                            backgroundHeat = max(backgroundHeat, heat * 0.6);
                        } else {
                            totalInfluence += influence * 0.7;
                            maxHeat = max(maxHeat, heat);
                        }
                    }
                }
            }
        }

        totalInfluence = totalInfluence + backgroundInfluence * 0.5;
        totalInfluence = clamp(totalInfluence, 0.0, 1.5);

        if (maxHeat < 0.1 && backgroundHeat > 0.01) {
            maxHeat = backgroundHeat;
        }

        if (totalInfluence > 0.01) {
            vec3 finalColor;

            if (isBlockEffect) {
                vec3 brightBlue = vec3(0.7, 0.9, 1.0);
                vec3 coreBlue = vec3(0.3, 0.6, 0.9);
                vec3 deepBlue = vec3(0.1, 0.3, 0.7);
                vec3 greyBlue = vec3(0.2, 0.3, 0.4);
                vec3 darkGrey = vec3(0.1, 0.15, 0.2);

                float shimmer = sin(localPos2D.x * 60.0 + localPos2D.y * 40.0) * 0.1;
                maxHeat += shimmer;

                if (maxHeat > 0.8) {
                    finalColor = mix(coreBlue, brightBlue, (maxHeat - 0.8) * 5.0);
                } else if (maxHeat > 0.5) {
                    finalColor = mix(deepBlue, coreBlue, (maxHeat - 0.5) * 3.33);
                } else if (maxHeat > 0.3) {
                    finalColor = mix(greyBlue, deepBlue, (maxHeat - 0.3) * 5.0);
                } else {
                    finalColor = mix(darkGrey, greyBlue, maxHeat * 3.33);
                }

                // cool glow
                finalColor *= 1.0 + totalInfluence * 0.3;
            } else {
                vec3 white = vec3(1.0, 1.0, 0.9);
                vec3 yellow = vec3(1.0, 0.8, 0.2);
                vec3 orange = vec3(1.0, 0.4, 0.0);
                vec3 red = vec3(0.8, 0.1, 0.0);
                vec3 darkRed = vec3(0.4, 0.05, 0.0);

                float colorNoise = sin(localPos2D.x * 40.0) * cos(localPos2D.y * 40.0) * 0.05;
                maxHeat += colorNoise;

                if (maxHeat > 0.7) {
                    finalColor = mix(yellow, white, (maxHeat - 0.7) * 3.33);
                } else if (maxHeat > 0.4) {
                    finalColor = mix(orange, yellow, (maxHeat - 0.4) * 3.33);
                } else if (maxHeat > 0.2) {
                    finalColor = mix(red, orange, (maxHeat - 0.2) * 5.0);
                } else {
                    finalColor = mix(darkRed, red, maxHeat * 5.0);
                }

                // emissive glow
                finalColor *= 1.0 + totalInfluence * 0.5;
            }

            color = vec4(finalColor, totalInfluence);
        } else {
            discard;
        }

        return;
    }

    // edge
    float edgealpha = boxalpha;
    edgealpha = max(edgealpha, edge(abs(vertex_pos.x - offset.x)));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.x - (offset.x+size.x))));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.y - offset.y)));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.y - (offset.y+size.y))));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.z - offset.z)));
    edgealpha = max(edgealpha, edge( abs(vertex_pos.z - (offset.z+size.z))));

    color.a = edgealpha;
}
