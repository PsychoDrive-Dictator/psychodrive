#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define GLSL_VER "#version 300 es"
#include <emscripten/html5.h>
#else
#include "gl3w.h"
#define GLSL_VER "#version 330"
#endif

#define _USE_MATH_DEFINES

#include <numbers>
#include <cmath>
#include <random>

#include "render.hpp"
#include "ui.hpp"
#include "guy.hpp"

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

SDL_Window* window = nullptr;
SDL_GLContext gl_context;

float projMatrix[16];
float viewMatrix[16];
GLuint vbo;
GLuint vao;
GLuint program;

GLuint loc_attrib;
GLuint loc_view;
GLuint loc_proj;
GLuint loc_size;
GLuint loc_offset;
GLuint loc_color;
GLuint loc_isgrid;
GLuint loc_progress;
GLuint loc_particleTexture;

static int current_isgrid = -1;

static void setIsGrid(int value)
{
    if (current_isgrid != value) {
        glUniform1i(loc_isgrid, value);
        current_isgrid = value;
    }
}

void crossProduct( float *a, float *b, float *res)
{
    res[0] = a[1] * b[2]  -  b[1] * a[2];
    res[1] = a[2] * b[0]  -  b[2] * a[0];
    res[2] = a[0] * b[1]  -  b[0] * a[1];
}

void normalize(float *a)
{
    float mag = sqrt(a[0] * a[0]  +  a[1] * a[1]  +  a[2] * a[2]);
    a[0] /= mag;
    a[1] /= mag;
    a[2] /= mag;
}

void setIdentityMatrix( float *mat, int size = 4)
{
    // fill matrix with 0s
    for (int i = 0; i < size * size; ++i)
            mat[i] = 0.0f;

    // fill diagonal with 1s
    for (int i = 0; i < size; ++i)
        mat[i + i * size] = 1.0f;
}

// a *= b
void multMatrix(float *a, float *b)
{
    float res[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            res[j*4 + i] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                res[j*4 + i] += a[k*4 + i] * b[j*4 + k];
            }
        }
    }
    memcpy(a, res, 16 * sizeof(float));
}

void setTranslationMatrix(float *mat, float x, float y, float z)
{
    setIdentityMatrix(mat,4);
    mat[12] = x;
    mat[13] = y;
    mat[14] = z;
}

void buildProjectionMatrix(float fov, float ratio, float nearP, float farP)
{
    float tangent = tan(fov/2 * std::numbers::pi / 180.0); // tangent of half fovX
    float right = nearP * tangent; // half width of near plane
    float top = right / ratio; // half height of near plane

    setIdentityMatrix(projMatrix,4);
    projMatrix[0] = nearP / right;
    projMatrix[5] = nearP / top;
    projMatrix[10] = -(farP + nearP) / (farP - nearP);
    projMatrix[11] = -1.0;
    projMatrix[14] = -(2.0 * farP * nearP) / (farP - nearP);
    projMatrix[15] = 0.0;
}

void setCamera(float posX, float posY, float posZ,
               float lookAtX, float lookAtY, float lookAtZ)
{
    float dir[3], right[3], up[3];

    up[0] = 0.0f;   up[1] = 1.0f;   up[2] = 0.0f;

    dir[0] =  (lookAtX - posX);
    dir[1] =  (lookAtY - posY);
    dir[2] =  (lookAtZ - posZ);
    normalize(dir);

    crossProduct(dir,up,right);
    normalize(right);

    crossProduct(right,dir,up);
    normalize(up);

    float aux[16];

    viewMatrix[0]  = right[0];
    viewMatrix[4]  = right[1];
    viewMatrix[8]  = right[2];
    viewMatrix[12] = 0.0f;

    viewMatrix[1]  = up[0];
    viewMatrix[5]  = up[1];
    viewMatrix[9]  = up[2];
    viewMatrix[13] = 0.0f;

    viewMatrix[2]  = -dir[0];
    viewMatrix[6]  = -dir[1];
    viewMatrix[10] = -dir[2];
    viewMatrix[14] =  0.0f;

    viewMatrix[3]  = 0.0f;
    viewMatrix[7]  = 0.0f;
    viewMatrix[11] = 0.0f;
    viewMatrix[15] = 1.0f;

    setTranslationMatrix(aux, -posX, -posY, -posZ);

    multMatrix(viewMatrix, aux);
}

const float cube[] = {
    // front
    0.0, 0.0, 1.0,
    1.0, 0.0, 1.0,
    1.0, 1.0, 1.0,
    0.0, 0.0, 1.0,
    1.0, 1.0, 1.0,
    0.0, 1.0, 1.0,
    // back
    0.0, 0.0, 0.0,
    1.0, 0.0, 0.0,
    1.0, 1.0, 0.0,
    0.0, 0.0, 0.0,
    1.0, 1.0, 0.0,
    0.0, 1.0, 0.0,
    // bottom
    0.0, 0.0, 0.0,
    1.0, 0.0, 0.0,
    0.0, 0.0, 1.0,
    1.0, 0.0, 0.0,
    0.0, 0.0, 1.0,
    1.0, 0.0, 1.0,
    // left
    0.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0,
    0.0, 1.0, 1.0,
    // right
    1.0, 0.0, 0.0,
    1.0, 1.0, 0.0,
    1.0, 0.0, 1.0,
    1.0, 1.0, 0.0,
    1.0, 0.0, 1.0,
    1.0, 1.0, 1.0,
    // top
    0.0, 1.0, 0.0,
    1.0, 1.0, 0.0,
    0.0, 1.0, 1.0,
    1.0, 1.0, 0.0,
    0.0, 1.0, 1.0,
    1.0, 1.0, 1.0,
};

void drawBoxInternal( float x, float y, float w, float h, float thickness, float r, float g, float b, float a, bool noFront = false )
{
    glUniform3f(loc_size, w, h, thickness);
    glUniform3f(loc_offset, x, y, -thickness/2.0);
    glUniform4f(loc_color, r, g, b, a);

    glBindVertexArray(vao);
    GLint first = noFront ? 6 : 0;
    GLsizei count = IM_ARRAYSIZE(cube) / 3 - first;
    glDrawArrays(GL_TRIANGLES, first, count);
}

void drawBox( float x, float y, float w, float h, float thickness, float r, float g, float b, float a, bool noFront /*= false*/ )
{
    setIsGrid(0);
    drawBoxInternal(x, y, w, h, thickness, r, g, b, a, noFront);
}

void drawHitBox(Box box, float thickness, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ )
{
    setIsGrid(0);
    if (isDrive || isParry || isDI ) {
        float driveOffset = 0.75;
        float colorR = 0.0;
        float colorG = 0.6;
        float colorB = 0.1;
        if (isParry) {
            colorR = 0.3;
            colorG = 0.7;
            colorB = 0.9;
        }
        if (isDI) {
            colorR = 0.9;
            colorG = 0.0;
            colorB = 0.0;
        }
        drawBoxInternal( box.x.f()-driveOffset, box.y.f()-driveOffset, box.w.f()+driveOffset*2, box.h.f()+driveOffset*2,thickness+driveOffset*2,colorR,colorG,colorB,0.2);
    }
    drawBoxInternal( box.x.f(), box.y.f(), box.w.f(), box.h.f(),thickness,col.r,col.g,col.b,0.2);
}

bool thickboxes = false;
bool renderPositionAnchors = true;
float zoom = 500.0;
float fov = 80.0;
int translateX = 0.0;
int translateY = 150.0;

static GLuint
compile_shader(GLenum type, const GLchar *source)
{
    GLuint shader = glCreateShader(type);
    std::string strShaderSource = std::string(GLSL_VER) + "\n\n" + std::string(source);
    const char *pSrcString = strShaderSource.c_str();
    glShaderSource(shader, 1, &pSrcString, NULL);
    glCompileShader(shader);
    GLint param;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &param);
    if (!param) {
        GLchar log[4096];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "error: %s: %s\n",
                type == GL_FRAGMENT_SHADER ? "frag" : "vert", (char *) log);
        exit(EXIT_FAILURE);
    }
    return shader;
}

static GLuint
link_program(GLuint vert, GLuint frag)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    GLint param;
    glGetProgramiv(program, GL_LINK_STATUS, &param);
    if (!param) {
        GLchar log[4096];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "error: link: %s\n", (char *) log);
        exit(EXIT_FAILURE);
    }
    return program;
}

std::vector<HitMarker> vecMarkers;

void addHitMarker(HitMarker newMarker)
{
    vecMarkers.push_back(newMarker);
}

void clearHitMarkers(void)
{
    vecMarkers.clear();
}

void drawHitMarker(float x, float y, float radius, int hitType, int time, int maxTime, float dirX, float dirY, int seed)
{
    const int kParticleCount = 256;
    const int kTextureSize = int(sqrt(kParticleCount));

    const int kBlockSphereParticles = 128;
    const int kProjectileParticles = 64;
    const int kProjectileStreaks = 8;

    float particleData[kParticleCount * 4];

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // pre-generate random values and sizes for projectile streaks
    float streakRandValues[kProjectileStreaks][2];
    int streakSizes[kProjectileStreaks];
    int totalStreakParticles = 0;
    
    for (int s = 0; s < kProjectileStreaks - 1; s++) {
        streakRandValues[s][0] = dist(rng);
        streakRandValues[s][1] = dist(rng);
        // random size between 6-10 particles per streak
        streakSizes[s] = 6 + int(dist(rng) * 5);
        totalStreakParticles += streakSizes[s];
    }
    
    // last streak gets remaining particles
    streakRandValues[kProjectileStreaks - 1][0] = dist(rng);
    streakRandValues[kProjectileStreaks - 1][1] = dist(rng);
    streakSizes[kProjectileStreaks - 1] = kProjectileParticles - totalStreakParticles;

    for (int i = 0; i < kParticleCount; i++) {
        float timeNorm = float(time + 6) / float(maxTime + 6);
        if (hitType == 2) {
            timeNorm = float(time) / float(maxTime);
        }

        float particleType = dist(rng);
        float speed, fadeRate, dirBias;
        bool isProjectile = false;
        bool isBackground = false;
        float angle;

        if (hitType == 2 && i < kBlockSphereParticles) {
            isProjectile = true;

            float impactTheta = atan2(dirY, dirX);
            float impactPhi = 0.0f;

            const float kSphereSpread = 0.8f;
            float spreadTheta = (dist(rng) - 0.5f) * kSphereSpread;
            float spreadPhi = (dist(rng) - 0.5f) * kSphereSpread;

            angle = impactTheta + spreadTheta;

            const float kPhiEncodingScale = 0.2f;
            particleData[i * 4 + 2] = 0.5f + (impactPhi + spreadPhi) * kPhiEncodingScale;

            float flowDir = dist(rng) * M_PI * 2;
            speed = 0.4f + dist(rng) * 0.3f;
            fadeRate = 0.8f + dist(rng) * 0.4f;
            dirBias = flowDir;

            isBackground = true;
        } else if (i < kProjectileParticles) {
            isProjectile = true;

            int particlesSoFar = 0;
            int streakIndex = 0;

            for (int s = 0; s < kProjectileStreaks; s++) {
                if (i < particlesSoFar + streakSizes[s]) {
                    streakIndex = s;
                    isBackground = (s == 0 || s == 2 || s == 5 || s == 7);
                    break;
                }
                particlesSoFar += streakSizes[s];
            }
            int particleInStreak = i - particlesSoFar;

            float streakRand = streakRandValues[streakIndex][0];
            float streakRand2 = streakRandValues[streakIndex][1];

            float hitAngle = atan2(dirY, dirX);
            float spreadAngle = (float(streakIndex) / float(kProjectileStreaks) - 0.5f) * M_PI;
            const float kAngleVariation = 0.4f;
            float angleVariation = (streakRand - 0.5f) * kAngleVariation;

            angle = hitAngle + spreadAngle + angleVariation;

            const float kMinStreakVelocity = 0.5f;
            const float kMaxStreakVelocityRange = 0.9f;
            float streakVelocity = kMinStreakVelocity + streakRand2 * kMaxStreakVelocityRange;

            float stagger = float(particleInStreak) / float(streakSizes[streakIndex]);
            const float kTrailSpeedReduction = 0.25f;
            const float kTrailFadeIncrease = 0.3f;
            speed = streakVelocity * (1.0f - stagger * kTrailSpeedReduction);
            fadeRate = 0.5f + stagger * kTrailFadeIncrease;
            dirBias = 0.0f;
        } else if (particleType < 0.3f) {
            speed = 1.0f + dist(rng) * 0.3f;
            fadeRate = 1.2f;
            dirBias = 0.5f + dist(rng) * 0.2f;
        } else if (particleType < 0.7f) {
            speed = 0.6f + dist(rng) * 0.2f;
            fadeRate = 0.9f;
            dirBias = 0.3f + dist(rng) * 0.2f;
        } else {
            speed = 0.3f + dist(rng) * 0.15f;
            fadeRate = 0.6f;
            dirBias = 0.1f;
        }

        const float kConeSpread = 0.8f;
        float angleSpread = isProjectile ? 0.0f : (dist(rng) - 0.5f) * kConeSpread;
        if (!isProjectile) {
            angle = dist(rng) * M_PI * 2 + angleSpread;
        }

        float burstDirX = cos(angle) * (1.0f - dirBias) + dirX * dirBias;
        float burstDirY = sin(angle) * (1.0f - dirBias) + dirY * dirBias;

        float dirLen = sqrt(burstDirX * burstDirX + burstDirY * burstDirY);
        burstDirX /= dirLen;
        burstDirY /= dirLen;

        const float kMinMotionPower = 1.5f;
        const float kMotionPowerRange = 0.5f;
        float motionPower = isProjectile ? 1.0f : (kMinMotionPower + dist(rng) * kMotionPowerRange);
        float motionCurve = pow(timeNorm, motionPower);
        float distance = speed * motionCurve;

        float turbulence = 0.0f;
        if (!isProjectile) {
            const float kBaseTurbFreq = 12.0f;
            const float kTurbFreqRange = 8.0f;
            const float kBaseTurbAmp = 0.06f;
            const float kTurbAmpRange = 0.08f;
            const float kSecondaryTurbFreq = 18.0f;
            const float kSecondaryTurbFreqRange = 12.0f;
            const float kSecondaryTurbScale = 0.75f;

            float turbFreq = kBaseTurbFreq + dist(rng) * kTurbFreqRange;
            float turbAmplitude = kBaseTurbAmp + dist(rng) * kTurbAmpRange;
            turbulence = sin(timeNorm * turbFreq + dist(rng) * M_PI * 2) * turbAmplitude;

            float turbFreq2 = kSecondaryTurbFreq + dist(rng) * kSecondaryTurbFreqRange;
            turbulence += cos(timeNorm * turbFreq2 + dist(rng) * M_PI * 2) * turbAmplitude * kSecondaryTurbScale;
        }

        const float kSphereRadius = 0.30f;
        const float kSphereYScale = 1.5f; // akshually it's an ellipsoid
        const float kWorldScale = 0.35f;
        const float kPhiDecodeScale = 0.2f;
        const float kPerspectiveScaleFactor = 0.3f;
        const float kSphereCenterX = 0.8f;
        const float kBrightnessBase = 0.4f;
        const float kBrightnessRange = 0.25f;
        const float kBrightnessDecay = 0.2f;
        const float kFlowSpeed = 1.0f;

        if (hitType == 2 && i < kBlockSphereParticles) {
            float theta0 = angle;
            float phi0 = (particleData[i * 4 + 2] - 0.5f) / kPhiDecodeScale;
            float flowDirection = dirBias;

            float arcAngle = timeNorm * kFlowSpeed;

            float theta = theta0 + cos(flowDirection) * arcAngle;
            float phi = phi0 + sin(flowDirection) * arcAngle;

            phi = fmax(-M_PI / 2, fmin(M_PI / 2, phi));

            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            float cosPhi = cos(phi);
            float sinPhi = sin(phi);

            float x3d = kSphereRadius * cosPhi * cosTheta;
            float y3d = kSphereRadius * kSphereYScale * cosPhi * sinTheta;
            float z3d = kSphereRadius * sinPhi;

            float perspectiveScale = 1.0f + z3d * kPerspectiveScaleFactor;

            particleData[i * 4 + 0] = kSphereCenterX + (dirX < 0 ? x3d : -x3d) * perspectiveScale;
            if (dirX < 0) {
                particleData[i * 4 + 0] = 1.0f - particleData[i * 4 + 0];
            }
            particleData[i * 4 + 1] = 0.5f + y3d * perspectiveScale;

            float facingCamera = -x3d * dirX + y3d * dirY;
            float brightness = kBrightnessBase + facingCamera * kBrightnessRange;
            particleData[i * 4 + 2] = brightness * (1.0f - arcAngle * kBrightnessDecay);
        } else if (hitType == 2 && i >= kBlockSphereParticles) {
            const float kBlockBrightnessBase = 0.4f;
            const float kBlockBrightnessRange = 0.3f;

            particleData[i * 4 + 0] = 0.5f + (burstDirX + turbulence) * distance * kWorldScale;
            particleData[i * 4 + 1] = 0.5f + burstDirY * distance * kWorldScale;
            particleData[i * 4 + 2] = kBlockBrightnessBase + dist(rng) * kBlockBrightnessRange;
        } else if (isProjectile) {
            const float kBgBrightnessBase = 0.2f;
            const float kBgBrightnessRange = 0.1f;
            const float kFgBrightnessBase = 0.7f;
            const float kFgBrightnessRange = 0.2f;

            particleData[i * 4 + 0] = 0.5f + burstDirX * distance * kWorldScale;
            particleData[i * 4 + 1] = 0.5f + burstDirY * distance * kWorldScale;
            particleData[i * 4 + 2] = isBackground ?
                (kBgBrightnessBase + dist(rng) * kBgBrightnessRange) :
                (kFgBrightnessBase + dist(rng) * kFgBrightnessRange);
        } else {
            const float kColorVariationBase = 0.5f;
            const float kColorVariationRange = 0.4f;

            particleData[i * 4 + 0] = 0.5f + (burstDirX + turbulence) * distance * kWorldScale;
            particleData[i * 4 + 1] = 0.5f + burstDirY * distance * kWorldScale;
            particleData[i * 4 + 2] = kColorVariationBase + dist(rng) * kColorVariationRange;
        }

        float fade;
        if (hitType == 2) {
            const int kBlockFastFadeLimit = 80;
            const float kBlockSlowFadeScale = 0.7f;

            if (i < kBlockFastFadeLimit) {
                fade = pow(1.0f - timeNorm, fadeRate);
            } else {
                fade = pow(1.0f - timeNorm * kBlockSlowFadeScale, fadeRate);
            }
        } else if (isProjectile) {
            int particlesSoFar = 0;
            int streakIndex = 0;
            for (int s = 0; s < kProjectileStreaks; s++) {
                if (i < particlesSoFar + streakSizes[s]) {
                    streakIndex = s;
                    break;
                }
                particlesSoFar += streakSizes[s];
            }
            int particleInStreak = i - particlesSoFar;
            float stagger = float(particleInStreak) / float(streakSizes[streakIndex]);

            const float kProjectileFadeThreshold = 0.7f;
            const float kFadeRange = 0.3f;
            const float kFadeEncodingScale = 0.8f;
            const float kStaggerEncodingScale = 0.2f;

            float timeFade = (timeNorm < kProjectileFadeThreshold) ?
                1.0f :
                pow(1.0f - (timeNorm - kProjectileFadeThreshold) / kFadeRange, 2.0f);
            fade = timeFade * kFadeEncodingScale + stagger * kStaggerEncodingScale;
        } else {
            fade = pow(1.0f - timeNorm, fadeRate);
        }
        particleData[i * 4 + 3] = fade;
    }

    GLuint particleTexture;
    glGenTextures(1, &particleTexture);
    glBindTexture(GL_TEXTURE_2D, particleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kTextureSize, kTextureSize, 0, GL_RGBA, GL_FLOAT, particleData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    color col = {0.9f, 0.4f, 0.3f};
    float alpha = 0.9f;
    if (hitType == 2) {
        col = {0.55f, 0.7f, 0.8f};
        alpha = 0.75f;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, particleTexture);
    glUniform1i(loc_particleTexture, 0);

    setIsGrid(2);
    glUniform1i(loc_progress, time);

    const float kQuadSizeMultiplier = 3.0f;
    float quadSize = radius * kQuadSizeMultiplier;
    glUniform3f(loc_size, quadSize, quadSize, quadSize);
    glUniform3f(loc_offset, x - quadSize * 0.5f, y - quadSize * 0.5f, 0);
    glUniform4f(loc_color, col.r, col.g, col.b, alpha);

    glBindVertexArray(vao);
    const int kFrontFaceStart = 6;
    const int kFrontFaceCount = 6;
    glDrawArrays(GL_TRIANGLES, kFrontFaceStart, kFrontFaceCount);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &particleTexture);
}

void renderMarkersAndStuff(void)
{
    int markerToDelete = -1;
    std::vector<Guy *> everyone;
    gatherEveryone(guys, everyone);
    for (uint32_t i = 0; i < vecMarkers.size(); i++)
    {
        if (std::find(everyone.begin(), everyone.end(), vecMarkers[i].pOrigin) == everyone.end()) {
            // in case it already got deleted (by sim snapshot or otherwise)
            continue;
        }
        float hitMarkPosX = vecMarkers[i].pOrigin->getPosX().f() + vecMarkers[i].x;
        float hitMarkPosY = vecMarkers[i].pOrigin->getPosY().f() + vecMarkers[i].y;

        drawHitMarker(hitMarkPosX, hitMarkPosY, vecMarkers[i].radius,
                      vecMarkers[i].type, vecMarkers[i].time, vecMarkers[i].maxtime,
                      vecMarkers[i].dirX, vecMarkers[i].dirY, vecMarkers[i].seed);

        if (!vecMarkers[i].pOrigin->getHitStop()) {
            vecMarkers[i].time++;
        }
        if (vecMarkers[i].time > vecMarkers[i].maxtime) {
            markerToDelete = i;
        }
    }
    if (markerToDelete != -1) {
        vecMarkers.erase(vecMarkers.begin()+markerToDelete);
    }
}

void setRenderState(color clearColor, int sizeX, int sizeY)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    //glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef __EMSCRIPTEN__
    emscripten_get_canvas_element_size("#canvas", &sizeX, &sizeY);
#endif

    glViewport(0, 0, sizeX, sizeY);
    buildProjectionMatrix(fov, (float)sizeX / (float)sizeY, 1.0, 3000.0);
    setCamera(translateX, translateY, zoom, translateX, translateY, -1000.0);

    glUseProgram(program);

    glUniformMatrix4fv(loc_proj, 1, false, projMatrix);
    glUniformMatrix4fv(loc_view, 1, false, viewMatrix);

    // render stage
    setIsGrid(1);
    drawBoxInternal(-800.0, 0.0, 1600.0, 500.0, 1000.0, 1.0,1.0,1.0,1.0, true);
}

void setScreenSpaceRenderState(int sizeX, int sizeY)
{
    setIdentityMatrix(projMatrix,4);

    projMatrix[0] = 2.0 / sizeX;
    projMatrix[5] = 2.0 / -sizeY;
    projMatrix[10] = 0.1;
    projMatrix[12] = -1.0;
    projMatrix[13] = 1.0;
    projMatrix[14] = 0.0;

    setIdentityMatrix(viewMatrix,4);

    glUniformMatrix4fv(loc_proj, 1, false, projMatrix);
    glUniformMatrix4fv(loc_view, 1, false, viewMatrix);
}

SDL_Window* initWindowRender()
{
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    // this somehow takes ages on my home machine so putting it behind a flag - multiple seconds on startup
    const char *pEnv = getenv("ENABLE_CONTROLLER");
    auto SDL_init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER;
    if ( pEnv && pEnv[0] == '0' ) SDL_init_flags &= ~SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SDL_init_flags ) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return nullptr;
    }

#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    std::string strWindowTitle = "Psycho Drive " + std::string(STRINGIZE_VALUE_OF(PROJECT_VERSION));

    int w = 1920, h = 1080;

#ifdef __EMSCRIPTEN__
    emscripten_get_canvas_element_size("#canvas", &w, &h);
#endif

    window = SDL_CreateWindow(strWindowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return nullptr;
    }

    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(0);

#ifndef __EMSCRIPTEN__
    if (gl3wInit()) {
        fprintf(stderr, "gl3w: failed to initialize\n");
        exit(EXIT_FAILURE);
    }
#endif

    /* Compile and link OpenGL program */
    GLuint vert = compile_shader(GL_VERTEX_SHADER, readFile("./data/shaders/vert.glsl").c_str());
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, readFile("./data/shaders/frag.glsl").c_str());
    program = link_program(vert, frag);
    loc_attrib = glGetAttribLocation(program, "in_pos");
    loc_view = glGetUniformLocation(program, "view");
    loc_proj = glGetUniformLocation(program, "proj");
    loc_size = glGetUniformLocation(program, "size");
    loc_offset = glGetUniformLocation(program, "offset");
    loc_color = glGetUniformLocation(program, "in_color");
    loc_isgrid = glGetUniformLocation(program, "isGrid");
    loc_progress = glGetUniformLocation(program, "progress");
    loc_particleTexture = glGetUniformLocation(program, "particleTexture");
    glDeleteShader(frag);
    glDeleteShader(vert);

    /* Prepare vertex buffer object (VBO) */
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Prepare vertrex array object (VAO) */
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(loc_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(loc_attrib);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return window;
}

void initRenderUI(void)
{
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(GLSL_VER);
}

void destroyRender(void)
{
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
