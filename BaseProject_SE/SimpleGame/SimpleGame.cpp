/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software under the What The Hell License.
*/
#include "stdafx.h"
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <memory>
#include <algorithm>
#include "Dependencies/glew.h"
#include "Dependencies/freeglut.h"
#include "Renderer.h"
#include "TutorialGame.h"

namespace {
std::unique_ptr<Renderer> renderer;
std::unique_ptr<TutorialGame> game;
int previousTime=0;
bool visible=true;
HWND gameWindow=nullptr;
void Display(){if(game){game->Draw();glutSwapBuffers();}}
void Resize(int w,int h){if(renderer)renderer->Resize(w,h);}
void KeyDown(unsigned char key,int,int){if(game)game->Key(key,true);}
void KeyUp(unsigned char key,int,int){if(game)game->Key(key,false);}
void Mouse(int button,int state,int x,int y){if(game){game->Aim(renderer->MousePosition(x,y));game->Mouse(button,state==GLUT_DOWN);}}
void Motion(int x,int y){if(game)game->Aim(renderer->MousePosition(x,y));}
void Visibility(int state){visible=state==GLUT_VISIBLE;if(game)game->ClearInput();}
void Close(){game.reset();renderer.reset();}
void Tick(int){
    if(!game)return;
    int now=glutGet(GLUT_ELAPSED_TIME);
    float elapsed=std::min(.10f,std::max(0.f,(now-previousTime)/1000.f));previousTime=now;
    // Poll only our own window's focus. Clear held keys on focus loss to avoid stuck movement.
    bool focused=GetForegroundWindow()==gameWindow;
    if(!focused || !visible)game->ClearInput();
    else {
        while(elapsed>0){float step=std::min(elapsed,1.f/60.f);game->Update(step);elapsed-=step;}
    }
    glutPostRedisplay();glutTimerFunc(16,Tick,0);
}
}
int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitContextVersion(3,3);glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(1280,800);glutCreateWindow(" ");
    gameWindow=WindowFromDC(wglGetCurrentDC());
    SetWindowTextW(gameWindow,L"카다쿠 · 데스워치 튜토리얼");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental=GL_TRUE;
    GLenum status=glewInit();
    if(status!=GLEW_OK || !GLEW_VERSION_3_3){
        MessageBoxW(gameWindow,L"그래픽 초기화에 실패했습니다. OpenGL 3.3 지원과 그래픽 드라이버를 확인하십시오.",L"그래픽 초기화 오류",MB_OK|MB_ICONERROR);return 1;}
    // GLEW can leave GL_INVALID_ENUM when initializing a core context.
    while(glGetError()!=GL_NO_ERROR){}
    renderer.reset(new Renderer(1280,800));
    if(!renderer->IsInitialized()){MessageBoxW(gameWindow,L"렌더러 또는 후처리 버퍼를 만들지 못했습니다.",L"렌더링 초기화 오류",MB_OK|MB_ICONERROR);renderer.reset();return 1;}
    game.reset(new TutorialGame(*renderer));
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display);glutReshapeFunc(Resize);
    glutKeyboardFunc(KeyDown);glutKeyboardUpFunc(KeyUp);
    glutMouseFunc(Mouse);glutMotionFunc(Motion);glutPassiveMotionFunc(Motion);
    glutVisibilityFunc(Visibility);glutCloseFunc(Close);
    previousTime=glutGet(GLUT_ELAPSED_TIME);glutTimerFunc(16,Tick,0);
    glutMainLoop();
    return 0;
}
