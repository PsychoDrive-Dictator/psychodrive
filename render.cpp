#include "render.hpp"
#include "ui.hpp"

SDL_Window* window = nullptr;
SDL_GLContext gl_context;

void drawBox( float x, float y, float w, float h, float r, float g, float b)
{
    glColor4f(r,g,b, 0.2f);

    glBegin(GL_QUADS);

    glVertex2f(x, y);
    glVertex2i(x+w, y);
    glVertex2i(x+w, y+h);
    glVertex2i(x, y+h);

    glEnd();

    glColor4f(r,g,b, 1.0f);

    glBegin(GL_LINE_LOOP);

    glVertex2f(x, y);
    glVertex2i(x+w, y);
    glVertex2i(x+w, y+h);
    glVertex2i(x, y+h);

    glEnd();
}

void drawHitBox(Box box, color col, bool isDrive /*= false*/, bool isParry /*= false*/, bool isDI /*= false*/ )
{
    if (isDrive || isParry || isDI ) {
        int driveOffset = 5;
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
        drawBox( box.x-driveOffset, box.y-driveOffset, box.w+driveOffset*2, box.h+driveOffset*2,colorR,colorG,colorB);
    }
    drawBox( box.x, box.y, box.w, box.h,col.r,col.g,col.b );
}

void setRenderState(color clearColor, int sizeX, int sizeY)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    glViewport(0, 0, sizeX, sizeY);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, sizeX, sizeY, 0.0f, 0.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTranslatef(0.0f,sizeY,0.0f);
    glScalef(1.5f, -1.5f, 1.0f);
}

SDL_Window* initWindowRender()
{
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    // this somehow takes ages on my home machine so putting it behind a flag - multiple seconds on startup
    const char *pEnv = getenv("ENABLE_CONTROLLER");
    auto SDL_init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
    if ( pEnv ) SDL_init_flags |= SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SDL_init_flags ) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return nullptr;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Psycho Drive", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return nullptr;
    }

    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    return window;
}

void initRenderUI(void)
{
    const char* glsl_version = "#version 130";
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void destroyRender(void)
{
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}