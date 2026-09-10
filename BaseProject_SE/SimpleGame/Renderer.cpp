#include "stdafx.h"
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include "Renderer.h"
#include "RenderShaders.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace {
GLuint Shader(GLenum type, const char* source) {
    GLuint shader=glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader,1,&source,nullptr);
    glCompileShader(shader);
    GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[2048]={}; glGetShaderInfoLog(shader,2048,nullptr,log);
        std::cerr << log << std::endl; glDeleteShader(shader); return 0;
    }
    return shader;
}
GLuint Link(const char* vertex,const char* fragment){
    GLuint v=Shader(GL_VERTEX_SHADER,vertex),f=Shader(GL_FRAGMENT_SHADER,fragment);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]={};glGetProgramInfoLog(p,2048,nullptr,log);std::cerr<<log;glDeleteProgram(p);return 0;}return p;
}
}
Renderer::Renderer(int width,int height) {
    // Embedded shaders make launching the executable independent of its working directory.
    const char* vs=RenderShaders::SceneVertex;
    const char* fs=RenderShaders::SceneFragment;
    GLuint v=Shader(GL_VERTEX_SHADER,vs),f=Shader(GL_FRAGMENT_SHADER,fs);
    if (!v || !f) { if(v)glDeleteShader(v); if(f)glDeleteShader(f); return; }
    program=glCreateProgram();
    glAttachShader(program,v); glAttachShader(program,f); glLinkProgram(program);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok) {
        char log[2048]={};glGetProgramInfoLog(program,2048,nullptr,log);std::cerr<<log;
        glDeleteProgram(program);program=0;return;
    }
    textured=glGetUniformLocation(program,"textured");
    materialLocation=glGetUniformLocation(program,"material");clockLocation=glGetUniformLocation(program,"clock");
    glUseProgram(program);glUniform1i(glGetUniformLocation(program,"image"),0);
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,r));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,u));
    postProgram=Link(RenderShaders::PostVertex,RenderShaders::PostFragment);
    if(postProgram){
        postLocations.source=glGetUniformLocation(postProgram,"source");postLocations.bloom=glGetUniformLocation(postProgram,"bloom");
        postLocations.effect=glGetUniformLocation(postProgram,"effect");postLocations.direction=glGetUniformLocation(postProgram,"direction");
        postLocations.clock=glGetUniformLocation(postProgram,"clock");postLocations.injury=glGetUniformLocation(postProgram,"injury");
    }
    vertices.reserve(65536); Resize(width,height);
    targetsReady=postProgram && CreateTargets();
}
Renderer::~Renderer() {
    for(const auto& entry:textCache) glDeleteTextures(1,&entry.second.id);
    for(GLuint texture:ownedTextures)glDeleteTextures(1,&texture);
    glDeleteTextures(1,&sceneTexture);glDeleteTextures(2,bloomTexture);
    glDeleteFramebuffers(1,&sceneFbo);glDeleteFramebuffers(1,&resolveFbo);glDeleteFramebuffers(2,bloomFbo);
    glDeleteRenderbuffers(1,&sceneColor);if(postProgram)glDeleteProgram(postProgram);
    if(vbo)glDeleteBuffers(1,&vbo);
    if(vao)glDeleteVertexArrays(1,&vao);
    if(program)glDeleteProgram(program);
}
void Renderer::Resize(int w,int h) {
    w=std::max(w,1);h=std::max(h,1);windowH=h;
    float scale=std::min(w/1280.f,h/800.f);
    viewportW=std::max(1,int(1280*scale));viewportH=std::max(1,int(800*scale));
    viewportX=(w-viewportW)/2;viewportY=(h-viewportH)/2;
}
Vec2 Renderer::MousePosition(int x,int y) const {
    return Vec2((x-viewportX)*1280.f/viewportW,(y-(windowH-viewportY-viewportH))*800.f/viewportH);
}
void Renderer::Begin() {
    vertices.clear();worldPass=true;batchTexture=0;batchMode=batchMaterial=0;
    glBindFramebuffer(GL_FRAMEBUFFER,0);glClearColor(.012f,.020f,.024f,1);glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER,sceneFbo);glViewport(0,0,1280,800);glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_MULTISAMPLE);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::End(){if(worldPass)Composite(0);Flush();}
void Renderer::SelectBatch(GLuint texture,int mode,int material){
    // Preserve painter order. Only adjacent draws with identical state are combined.
    if(texture!=batchTexture||mode!=batchMode||material!=batchMaterial||vertices.size()>65536)Flush();
    batchTexture=texture;batchMode=mode;batchMaterial=material;
}
void Renderer::Flush() {
    if(vertices.empty())return;
    glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,batchTexture);glUniform1i(textured,batchMode);
    glUniform1i(materialLocation,batchMaterial);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));vertices.clear();
}
void Renderer::Triangle(Vec2 a,Vec2 b,Vec2 c,Color k) {
    SelectBatch();
    for(Vec2 p:{a,b,c})vertices.push_back({p.x,p.y,k.r,k.g,k.b,k.a,0,0});
}
void Renderer::Quad(Vec2 a,Vec2 b,Vec2 c,Vec2 d,Color k){Triangle(a,b,c,k);Triangle(a,c,d,k);}
void Renderer::Rect(float x,float y,float w,float h,Color k){Quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},k);}
void Renderer::Line(Vec2 a,Vec2 b,float width,Color k) {
    Vec2 d=b-a;float length=std::sqrt(d.x*d.x+d.y*d.y);if(length<.001f)return;
    Vec2 n(-d.y/length*width*.5f,d.x/length*width*.5f);Quad(a+n,b+n,b-n,a-n,k);
}
void Renderer::Ellipse(Vec2 p,float rx,float ry,Color k){
    for(int i=0;i<24;++i){float a=i*6.2831853f/24,b=(i+1)*6.2831853f/24;
        Triangle(p,p+Vec2(std::cos(a)*rx,std::sin(a)*ry),p+Vec2(std::cos(b)*rx,std::sin(b)*ry),k);}
}
Renderer::TextImage Renderer::CreateText(const std::wstring& value) {
    TextImage result;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return result;
    HFONT font=CreateFontW(-22,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    HGDIOBJ oldFont=font?SelectObject(dc,font):nullptr;
    SIZE size={};GetTextExtentPoint32W(dc,value.c_str(),static_cast<int>(value.size()),&size);
    result.width=std::max(1,static_cast<int>(size.cx+4));result.height=30;
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=result.width;info.bmiHeader.biHeight=-result.height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(bitmap && pixels){
        HGDIOBJ oldBitmap=SelectObject(dc,bitmap);
        PatBlt(dc,0,0,result.width,result.height,BLACKNESS);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
        TextOutW(dc,1,1,value.c_str(),static_cast<int>(value.size()));GdiFlush();
        std::vector<unsigned char> alpha(result.width*result.height);
        const unsigned char* bgra=static_cast<unsigned char*>(pixels);
        for(size_t i=0;i<alpha.size();++i)alpha[i]=bgra[i*4];
        glGenTextures(1,&result.id);glBindTexture(GL_TEXTURE_2D,result.id);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,result.width,result.height,0,GL_RED,GL_UNSIGNED_BYTE,alpha.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        SelectObject(dc,oldBitmap);DeleteObject(bitmap);
    }
    if(oldFont)SelectObject(dc,oldFont);if(font)DeleteObject(font);DeleteDC(dc);return result;
}
void Renderer::Text(float x,float y,const std::wstring& value,Color k,float scale) {
    if(value.empty())return;Flush();
    auto it=textCache.find(value);
    if(it==textCache.end()){
        // Dynamic timers must not grow the cache indefinitely during long sessions.
        if(textCache.size()>=256){
            auto oldest=std::min_element(textCache.begin(),textCache.end(),[](const auto& a,const auto& b){return a.second.lastUsed<b.second.lastUsed;});
            glDeleteTextures(1,&oldest->second.id);textCache.erase(oldest);
        }
        it=textCache.emplace(value,CreateText(value)).first;
    }
    it->second.lastUsed=++textClock;
    const TextImage& t=it->second;if(!t.id)return;SelectBatch(t.id,1);
    float w=t.width*scale,h=t.height*scale;
    vertices={{x,y,k.r,k.g,k.b,k.a,0,0},{x+w,y,k.r,k.g,k.b,k.a,1,0},{x+w,y+h,k.r,k.g,k.b,k.a,1,1},
        {x,y,k.r,k.g,k.b,k.a,0,0},{x+w,y+h,k.r,k.g,k.b,k.a,1,1},{x,y+h,k.r,k.g,k.b,k.a,0,1}};
    Flush();
}
void Renderer::DrawSolidRect(float x,float y,float,float size,float r,float g,float b,float a){Rect(640+x-size/2,400-y-size/2,size,size,{r,g,b,a});}

bool Renderer::CreateTargets(){
    glGenFramebuffers(1,&sceneFbo);glBindFramebuffer(GL_FRAMEBUFFER,sceneFbo);
    glGenRenderbuffers(1,&sceneColor);glBindRenderbuffer(GL_RENDERBUFFER,sceneColor);
    GLint samples=1;glGetIntegerv(GL_MAX_SAMPLES,&samples);samples=std::min(samples,4);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_RGBA16F,1280,800);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,sceneColor);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return false;
    auto target=[](GLuint& fbo,GLuint& texture,int w,int h){
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    };
    bool ok=target(resolveFbo,sceneTexture,1280,800);
    for(int i=0;i<2;++i)ok=target(bloomFbo[i],bloomTexture[i],320,200)&&ok;
    glBindFramebuffer(GL_FRAMEBUFFER,0);return ok;
}
void Renderer::SetTime(float time){Flush();glUseProgram(program);glUniform1f(clockLocation,time);}
void Renderer::Fullscreen(GLuint source,GLuint bloom,int effect,Vec2 direction,float time,float injury){
    glUseProgram(postProgram);glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,bloom);
    glUniform1i(postLocations.source,0);glUniform1i(postLocations.bloom,1);
    glUniform1i(postLocations.effect,effect);
    glUniform2f(postLocations.direction,direction.x,direction.y);
    glUniform1f(postLocations.clock,time);glUniform1f(postLocations.injury,injury);
    glDrawArrays(GL_TRIANGLES,0,3);
}
void Renderer::Composite(float time,float injury){
    if(!worldPass)return;Flush();worldPass=false;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,sceneFbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resolveFbo);
    glBlitFramebuffer(0,0,1280,800,0,0,1280,800,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glDisable(GL_BLEND);glViewport(0,0,320,200);glBindFramebuffer(GL_FRAMEBUFFER,bloomFbo[0]);
    Fullscreen(sceneTexture,0,0,{},time,0);
    for(int i=0;i<6;++i){int destination=(i+1)%2;glBindFramebuffer(GL_FRAMEBUFFER,bloomFbo[destination]);
        Fullscreen(bloomTexture[i%2],0,1,i%2?Vec2(0,1.f/200):Vec2(1.f/320,0),time,0);}
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(viewportX,viewportY,viewportW,viewportH);
    Fullscreen(sceneTexture,bloomTexture[0],2,{},time,injury);
    glActiveTexture(GL_TEXTURE0);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::SoftEllipse(Vec2 center,float rx,float ry,Color c){
    SelectBatch();
    // Smooth vertex alpha, not concentric opaque rings. Used for shadows and emissive light pools.
    const int segments=48;
    for(int i=0;i<segments;++i){float a=i*6.2831853f/segments,b=(i+1)*6.2831853f/segments;
        vertices.push_back({center.x,center.y,c.r,c.g,c.b,c.a,0,0});
        vertices.push_back({center.x+std::cos(a)*rx,center.y+std::sin(a)*ry,c.r,c.g,c.b,0,0,0});
        vertices.push_back({center.x+std::cos(b)*rx,center.y+std::sin(b)*ry,c.r,c.g,c.b,0,0,0});
    }
}
void Renderer::MaterialQuad(Vec2 a,Vec2 b,Vec2 c,Vec2 d,Vec2 ta,Vec2 tb,Vec2 tc,Vec2 td,int material,Color k){
    SelectBatch(0,3,material);
    const Vec2 points[]={a,b,c,a,c,d},uv[]={ta,tb,tc,ta,tc,td};
    for(int i=0;i<6;++i)vertices.push_back({points[i].x,points[i].y,k.r,k.g,k.b,k.a,uv[i].x,uv[i].y});
}
void Renderer::Sprite(GLuint texture,Vec2 p,Vec2 size,int col,int row,int columns,int rows,Color k){
    if(!texture)return;SelectBatch(texture,2);
    // Half-pixel inset at a nominal 2048 atlas suppresses adjacent-cell bleeding.
    float u0=float(col)/columns+.00025f,v0=float(row)/rows+.00025f;
    float u1=float(col+1)/columns-.00025f,v1=float(row+1)/rows-.00025f;
    auto edges=atlasRowEdges.find(texture);
    if(edges!=atlasRowEdges.end()&&row>=0&&row+1<static_cast<int>(edges->second.size())){
        v0=edges->second[row]+.00025f;v1=edges->second[row+1]-.00025f;
    }
    vertices.insert(vertices.end(),{{p.x,p.y,k.r,k.g,k.b,k.a,u0,v0},{p.x+size.x,p.y,k.r,k.g,k.b,k.a,u1,v0},{p.x+size.x,p.y+size.y,k.r,k.g,k.b,k.a,u1,v1},
        {p.x,p.y,k.r,k.g,k.b,k.a,u0,v0},{p.x+size.x,p.y+size.y,k.r,k.g,k.b,k.a,u1,v1},{p.x,p.y+size.y,k.r,k.g,k.b,k.a,u0,v1}});
}
GLuint Renderer::LoadTexture(const std::wstring& filename){
    // WIC decodes PNG and preserves straight alpha without a new third-party dependency.
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;
    IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;
    GLuint texture=0;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr)){
        wchar_t executable[32768]={};GetModuleFileNameW(nullptr,executable,32768);
        std::wstring path=executable;path=path.substr(0,path.find_last_of(L"\\/")+1)+L"Assets/"+filename;
        hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
        if(FAILED(hr)){path=L"Assets/"+filename;hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);}
    }
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    UINT w=0,h=0;if(SUCCEEDED(hr))hr=converter->GetSize(&w,&h);
    if(SUCCEEDED(hr)&&w>0&&h>0&&w<=8192&&h<=8192){
        std::vector<unsigned char> rgba(static_cast<size_t>(w)*h*4);
        hr=converter->CopyPixels(nullptr,w*4,static_cast<UINT>(rgba.size()),rgba.data());
        if(SUCCEEDED(hr)){
            glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            ownedTextures.push_back(texture);
            // The generated creature atlas has uneven row gutters. Use inspected UV boundaries,
            // preserving the original PNG and avoiding clipped Warrior crests or adjacent sprites.
            if(filename==L"tyranids-walk.png")atlasRowEdges[texture]={0.f,320.f/1086.f,678.f/1086.f,1.f};
        }
    }
    if(converter)converter->Release();if(frame)frame->Release();if(decoder)decoder->Release();if(factory)factory->Release();
    if(SUCCEEDED(com))CoUninitialize();
    if(!texture)std::wcerr<<L"그래픽 자산을 읽지 못했습니다: "<<filename<<std::endl;
    return texture;
}

void Renderer::SpriteShadow(GLuint texture,Vec2 ground,float width,float length,int col,int row,int columns,int rows,float opacity){
    if(!texture)return;SelectBatch(texture,2);
    float u0=float(col)/columns+.00025f,u1=float(col+1)/columns-.00025f;
    float v0=float(row)/rows+.00025f,v1=float(row+1)/rows-.00025f;
    auto edges=atlasRowEdges.find(texture);
    if(edges!=atlasRowEdges.end()&&row+1<static_cast<int>(edges->second.size())){v0=edges->second[row]+.00025f;v1=edges->second[row+1]-.00025f;}
    // Three offset alpha silhouettes soften the penumbra without introducing a 3D shadow-map dependency.
    for(int pass=-1;pass<=1;++pass){
        Vec2 offset(pass*1.7f,pass*.7f),a=ground+Vec2(-width*.5f+length,length*.36f)+offset;
        Vec2 b=ground+Vec2(width*.5f+length,length*.36f)+offset,c=ground+Vec2(width*.5f,0)+offset,d=ground+Vec2(-width*.5f,0)+offset;
        const Vec2 points[]={a,b,c,a,c,d},uv[]={{u0,v0},{u1,v0},{u1,v1},{u0,v0},{u1,v1},{u0,v1}};
        for(int i=0;i<6;++i)vertices.push_back({points[i].x,points[i].y,.003f,.007f,.006f,opacity/3,uv[i].x,uv[i].y});
    }
}
