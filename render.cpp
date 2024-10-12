#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define GLSL_VER "#version 300 es"
#else
#include "gl3w.h"
#define GLSL_VER "#version 330"
#endif

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

void crossProduct( float *a, float *b, float *res) {
 
    res[0] = a[1] * b[2]  -  b[1] * a[2];
    res[1] = a[2] * b[0]  -  b[2] * a[0];
    res[2] = a[0] * b[1]  -  b[0] * a[1];
}

void normalize(float *a) {
 
    float mag = sqrt(a[0] * a[0]  +  a[1] * a[1]  +  a[2] * a[2]);
 
    a[0] /= mag;
    a[1] /= mag;
    a[2] /= mag;
}

void setIdentityMatrix( float *mat, int size = 4) {
 
    // fill matrix with 0s
    for (int i = 0; i < size * size; ++i)
            mat[i] = 0.0f;
 
    // fill diagonal with 1s
    for (int i = 0; i < size; ++i)
        mat[i + i * size] = 1.0f;
}
 
// a *= b
void multMatrix(float *a, float *b) {
 
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
 
void setTranslationMatrix(float *mat, float x, float y, float z) {
 
    setIdentityMatrix(mat,4);
    mat[12] = x;
    mat[13] = y;
    mat[14] = z;
}
 
void buildProjectionMatrix(float fov, float ratio, float nearP, float farP) {
 
    float f = 1.0f / tan (fov * (M_PI / 360.0));
 
    setIdentityMatrix(projMatrix,4);
 
    projMatrix[0] = f / ratio;
    projMatrix[1 * 4 + 1] = f;
    projMatrix[2 * 4 + 2] = (farP + nearP) / (nearP - farP);
    projMatrix[3 * 4 + 2] = (2.0f * farP * nearP) / (nearP - farP);
    projMatrix[2 * 4 + 3] = -1.0f;
    projMatrix[3 * 4 + 3] = 0.0f;
}
 
void setCamera(float posX, float posY, float posZ,
               float lookAtX, float lookAtY, float lookAtZ) {
 
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
    0.0, 0.0, 0.0,
    1.0, 0.0, 0.0,
    1.0, 1.0, 0.0,
    0.0, 0.0, 0.0,
    1.0, 1.0, 0.0,
    0.0, 1.0, 0.0,
    // back
    0.0, 0.0, 1.0,
    1.0, 0.0, 1.0,
    1.0, 1.0, 1.0,
    0.0, 0.0, 1.0,
    1.0, 1.0, 1.0,
    0.0, 1.0, 1.0,
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

void drawBox( float x, float y, float w, float h, float thickness, float r, float g, float b, float a)
{
    glUniform3f(loc_size, w, h, thickness);
    glUniform3f(loc_offset, x, y, -thickness/2.0);
    glUniform4f(loc_color, r, g, b, a);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, IM_ARRAYSIZE(cube) / 3);
}

void drawHitBox(Box box, float thickness, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ )
{
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
        drawBox( box.x.f()-driveOffset, box.y.f()-driveOffset, box.w.f()+driveOffset*2, box.h.f()+driveOffset*2,thickness+driveOffset*2,colorR,colorG,colorB,1.0);
    }
    drawBox( box.x.f(), box.y.f(), box.w.f(), box.h.f(),thickness,col.r,col.g,col.b,1.0);
}

bool thickboxes = false;
bool renderPositionAnchors = true;
float zoom = 500.0;
float fov = 50.0;
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

void renderMarkersAndStuff(void)
{
    int markerToDelete = -1;
    glUniform1i(loc_isgrid, 2);
    for (uint32_t i = 0; i < vecMarkers.size(); i++)
    {
        float hitMarkPosX = vecMarkers[i].pOrigin->getPosX().f() + vecMarkers[i].x;
        float hitMarkPosY = vecMarkers[i].pOrigin->getPosY().f() + vecMarkers[i].y;
        int progress = vecMarkers[i].radius + vecMarkers[i].time + 4;
        color col = {0.9, 0.4, 0.3};
        float alpha = 0.9;
        if (vecMarkers[i].type==2) {
            col = {0.55, 0.7, 0.8};
            alpha = 0.75;
            progress = vecMarkers[i].radius + vecMarkers[i].maxtime / 2 - vecMarkers[i].time;
        }
        glUniform1i(loc_progress, progress);
        drawBox(hitMarkPosX - vecMarkers[i].radius, hitMarkPosY - vecMarkers[i].radius,
        vecMarkers[i].radius*2, vecMarkers[i].radius*2, vecMarkers[i].radius*2, col.r,col.g,col.b,alpha);
        if (!vecMarkers[i].pOrigin->getWarudo()) {
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
    glViewport(0, 0, sizeX, sizeY);
    buildProjectionMatrix(50.0, (float)sizeX / (float)sizeY, 1.0, 1000.0);
    setCamera(translateX, translateY, zoom, translateX, translateY, -1000.0);


    glUseProgram(program);

    glUniformMatrix4fv(loc_proj, 1, false, projMatrix);
    glUniformMatrix4fv(loc_view, 1, false, viewMatrix);

    // render stage
    glUniform1i(loc_isgrid, 1);
    drawBox(-800.0, 0.0, 1600.0, 500.0, 1000.0, 1.0,1.0,1.0,1.0);
    glUniform1i(loc_isgrid, 0);
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
    window = SDL_CreateWindow(strWindowTitle.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, window_flags);
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
