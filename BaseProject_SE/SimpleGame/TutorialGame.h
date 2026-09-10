#pragma once
#include "Renderer.h"
#include "NavigationGrid.h"
#include <random>

class TutorialGame {
public:
    explicit TutorialGame(Renderer& renderer);
    void Update(float dt);
    void Draw();
    void Key(unsigned char key,bool down);
    void Mouse(int button,bool down);
    void Aim(Vec2 screen){mouse=screen;}
    void ClearInput();
private:
    enum Phase { Approach,Recover,PassGrowth,FindWeapon,LoadPayload,ReachLift,Activate,Defend,Launch,LastStand,Ending };
    struct Prop { Vec2 p;float radius,height;int kind; };
    struct Enemy { Vec2 p;float hp,attack,flash;int group;int kind=0;float stagger=0,windup=0;bool warning=false; };
    struct Shot { Vec2 a,b;float life; };
    struct Particle { Vec2 p,v;float z,vz,life,total;Color color;float size; };
    struct Barrel { Vec2 p;bool alive=true; };
    struct Grenade { Vec2 start,end;float age; };
    Renderer& r;
    NavigationGrid navigation;
    float navigationTimer=0;
    std::mt19937 random;
    std::vector<Prop> props;
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<Particle> particles;
    std::vector<Barrel> barrels;
    std::vector<Grenade> grenades;
    GLuint marineAtlas=0,environmentAtlas=0,enemyAtlas=0;
    Vec2 player,camera,mouse=Vec2(740,400),facing=Vec2(1,0);
    bool keys[256]={},firing=false,paused=false,intro=true,dead=false,complete=false;
    bool encountered[11]={};
    bool moving=false,rifle=false,bossSpawned=false;
    int grenadesLeft=3,gunStrikes=0;
    float parryTime=0,parryCooldown=0,heavyCharge=0,footstep=0,injury=0;
    int stage=0,kills=0,magazine=24;
    float health=100,armor=100,time=0,stageTime=0,hold=0,defense=0;
    float gunCooldown=0,meleeCooldown=0,reload=0,damageDelay=0,dodgeCooldown=0,dodgeTime=0;
    float waveTimer=0,toastTime=0,walk=0,launchTime=0;
    Vec2 dodgeDirection;
    std::wstring toast;
    void Reset();
    float Random(float low,float high);
    Vec2 Project(Vec2 p,float height=0) const;
    Vec2 Target() const;
    bool Blocked(Vec2 p,float radius) const;
    void Move(Vec2& p,Vec2 delta,float radius);
    void Spawn(Vec2 center,int count,int group);
    void Shoot();
    void Melee();
    void Damage(float amount);
    void Advance();
    void Notify(const std::wstring& text);
    int NearbyEnemies(float radius) const;
    void Floor();
    void World();
    void Hud();
    void Box(Vec2 p,float sx,float sy,float height,Color c);
    void Marine(Vec2 p,bool corpse=false);
    void Tyranid(const Enemy& e);
    void DrawProp(const Prop& p);
    void Interaction(float dt);
    void Burst(Vec2 p,int count,Color color,float strength=1);
    void Blast(Vec2 p);
    void HeavyAttack();
    void DrawEffects();
    void Shadows();
    void RebuildNavigation();
};
