#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
#include <vector>
#include <map>
#include "Dependencies/glew.h"

struct Vec2 {
    float x, y;
    Vec2(float X = 0, float Y = 0) : x(X), y(Y) {}
    Vec2 operator+(Vec2 b) const { return Vec2(x + b.x, y + b.y); }
    Vec2 operator-(Vec2 b) const { return Vec2(x - b.x, y - b.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
};
struct Color {
    float r, g, b, a;
    Color(float R=1, float G=1, float B=1, float A=1) : r(R),g(G),b(B),a(A) {}
    Color Shade(float s) const { return Color(r*s,g*s,b*s,a); }
};

// Screen-space renderer. All coordinates use a letterboxed 1280 x 800 canvas.
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool IsInitialized() const { return program != 0 && vao != 0 && vbo != 0 && targetsReady; }
    void Resize(int width, int height);
    Vec2 MousePosition(int x, int y) const;
    void Begin();
    void Composite(float time, float injury=0);
    void End();
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);
    void Rect(float x, float y, float width, float height, Color color);
    void Line(Vec2 a, Vec2 b, float width, Color color);
    void Ellipse(Vec2 center, float rx, float ry, Color color);
    void SoftEllipse(Vec2 center, float rx, float ry, Color color);
    void MaterialQuad(Vec2 a,Vec2 b,Vec2 c,Vec2 d,Vec2 uvA,Vec2 uvB,Vec2 uvC,Vec2 uvD,int material,Color tint);
    void Sprite(GLuint texture,Vec2 topLeft,Vec2 size,int column,int row,int columns,int rows,Color tint=Color());
    void SpriteShadow(GLuint texture,Vec2 ground,float width,float length,int column,int row,int columns,int rows,float opacity);
    GLuint LoadTexture(const std::wstring& filename);
    void SetTime(float time);
    void Text(float x, float y, const std::wstring& value, Color color=Color(), float scale=1);
    void DrawSolidRect(float x,float y,float z,float size,float r,float g,float b,float a);
private:
    struct Vertex { float x,y,r,g,b,a,u,v; };
    struct TextImage { GLuint id=0; int width=0,height=0; unsigned long long lastUsed=0; };
    GLuint program=0, vao=0, vbo=0;
    GLuint postProgram=0,sceneFbo=0,sceneColor=0,resolveFbo=0,sceneTexture=0;
    GLuint bloomFbo[2]={},bloomTexture[2]={};
    bool targetsReady=false,worldPass=true;
    std::vector<GLuint> ownedTextures;
    std::map<GLuint,std::vector<float>> atlasRowEdges;
    GLint textured=-1;
    GLint materialLocation=-1,clockLocation=-1;
    struct PostLocations { GLint source=-1,bloom=-1,effect=-1,direction=-1,clock=-1,injury=-1; } postLocations;
    GLuint batchTexture=0;
    int batchMode=0,batchMaterial=0;
    unsigned long long textClock=0;
    int viewportX=0,viewportY=0,viewportW=1280,viewportH=800,windowH=800;
    std::vector<Vertex> vertices;
    std::map<std::wstring,TextImage> textCache;
    void Flush();
    void SelectBatch(GLuint texture=0,int mode=0,int material=0);
    bool CreateTargets();
    void Fullscreen(GLuint source,GLuint bloom,int effect,Vec2 direction,float time,float injury);
    TextImage CreateText(const std::wstring& value);
};
