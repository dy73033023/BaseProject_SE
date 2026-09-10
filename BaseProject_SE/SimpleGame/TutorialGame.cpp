#include "stdafx.h"
#include "TutorialGame.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace {
const float PI=3.14159265f,WorldSize=6000,DefenseSeconds=55;
const Vec2 Start(500,5100),Beacon(1000,5150),Wreck(1800,4600),Gate(4700,4500),Armory(4700,3500),Launcher(4700,1600),Lift(4950,1150),Console(5000,650);
const Color Gold(.79f,.65f,.38f),White(.83f,.85f,.78f),Muted(.52f,.61f,.56f),Red(.78f,.19f,.12f);
float Length(Vec2 a){return std::sqrt(a.x*a.x+a.y*a.y);}
Vec2 Unit(Vec2 a){float d=Length(a);return d>.001f?a*(1/d):Vec2();}
float Clamp(float x,float a,float b){return std::max(a,std::min(x,b));}
float SegmentDistance(Vec2 p,Vec2 a,Vec2 b){Vec2 d=b-a;float q=d.x*d.x+d.y*d.y;
 float t=q>.001f?Clamp(((p.x-a.x)*d.x+(p.y-a.y)*d.y)/q,0,1):0;return Length(p-(a+d*t));}
}
TutorialGame::TutorialGame(Renderer& renderer):r(renderer),random(40){
 marineAtlas=r.LoadTexture(L"titus-deathwatch-walk.png");environmentAtlas=r.LoadTexture(L"kadaku-environment.png");enemyAtlas=r.LoadTexture(L"tyranids-walk.png");Reset();
}
float TutorialGame::Random(float a,float b){return std::uniform_real_distribution<float>(a,b)(random);}
void TutorialGame::Reset(){
 random.seed(40);player=Start;camera=player;facing={1,0};health=armor=100;magazine=12;kills=stage=0;
 time=stageTime=hold=defense=gunCooldown=meleeCooldown=reload=damageDelay=0;
 dodgeCooldown=dodgeTime=waveTimer=toastTime=walk=launchTime=0;
 parryTime=parryCooldown=heavyCharge=footstep=injury=0;grenadesLeft=3;gunStrikes=0;
 moving=rifle=bossSpawned=false;intro=true;paused=dead=complete=false;ClearInput();
 enemies.clear();shots.clear();props.clear();particles.clear();barrels.clear();grenades.clear();
 for(bool& v:encountered)v=false;
 const Vec2 route[]={Start,Beacon,Wreck,Gate,Armory,Launcher,Lift,Console};
 for(int i=0;i<1300;++i){Vec2 p(Random(100,5900),Random(100,5900));float nearest=99999;
  for(int j=0;j<7;++j)nearest=std::min(nearest,SegmentDistance(p,route[j],route[j+1]));
  if(nearest<160 || Length(p-Console)<520 || Length(p-Launcher)<330)continue;
  int kind=i%9==0?2:(i%4==0?1:0);
  props.push_back({p,kind==2?45.f:kind==1?33.f:23.f,Random(90,190),kind});
 }
 for(int i=0;i<9;++i){props.push_back({{4330,700+i*140.f},40,185,2});props.push_back({{5350,700+i*140.f},40,185,2});}
 for(int i=0;i<8;++i)barrels.push_back({{Armory.x+(i%2?160.f:-170.f),Armory.y-250-i*280.f},true});
 RebuildNavigation();
 Notify(L"이동 키로 전진하십시오. 추락한 수송기를 찾아야 합니다.");
}
void TutorialGame::ClearInput(){for(bool& k:keys)k=false;firing=false;heavyCharge=0;moving=false;}
Vec2 TutorialGame::Project(Vec2 p,float h)const{Vec2 q=p-camera;return {640+(q.x-q.y)*.8f,438+(q.x+q.y)*.4f-h};}
Vec2 TutorialGame::Target()const{
 if(stage==Approach)return Beacon;if(stage==Recover)return Wreck;if(stage==PassGrowth)return Gate;
 if(stage==FindWeapon)return Armory;if(stage==LoadPayload)return Launcher;if(stage==ReachLift)return Lift;return Console;
}
bool TutorialGame::Blocked(Vec2 p,float radius)const{
 if(p.x<radius||p.y<radius||p.x>WorldSize-radius||p.y>WorldSize-radius)return true;
 if((stage<FindWeapon||p.x<4350+radius||p.x>5050-radius)&&std::abs(p.y-4200)<35+radius)return true;
 for(const Prop& prop:props)if(Length(prop.p-p)<prop.radius+radius)return true;
 return false;
}
void TutorialGame::Move(Vec2& p,Vec2 d,float radius){
 Vec2 x=p+Vec2(d.x,0);if(!Blocked(x,radius))p=x;Vec2 y=p+Vec2(0,d.y);if(!Blocked(y,radius))p=y;
}
void TutorialGame::RebuildNavigation(){
 navigation.Reset(100,60);
 for(const Prop& p:props)navigation.BlockCircle(p.p,p.radius+18);
 if(stage<FindWeapon)navigation.BlockRectangle({0,4145},{6000,4255});
 else {navigation.BlockRectangle({0,4145},{4370,4255});navigation.BlockRectangle({5030,4145},{6000,4255});}
 navigation.UpdateTarget(player);navigationTimer=.25f;
}
void TutorialGame::Notify(const std::wstring& s){toast=s;toastTime=6;}
void TutorialGame::Spawn(Vec2 center,int count,int group){
 for(int i=0;i<count;++i){Vec2 p;bool found=false;
  for(int attempt=0;attempt<60;++attempt){float a=Random(0,2*PI);p=center+Vec2(std::cos(a),std::sin(a))*Random(210,430);
   if(Blocked(p,18)||Length(p-player)<=155)continue;bool reachable=true;float distance=Length(center-p);Vec2 path=Unit(center-p);
   for(float d=0;d<distance;d+=16)if(Blocked(p+path*d,18)){reachable=false;break;}
   if(reachable){found=true;break;}
  }
  if(found){Enemy e={p,3,Random(.6f,1.5f),0,group};
   if(group>=PassGrowth&&i==0){e.kind=1;e.hp=11;}enemies.push_back(e);}
 }
}
void TutorialGame::Burst(Vec2 p,int count,Color c,float strength){
 for(int i=0;i<count&&particles.size()<700;++i){float a=Random(0,2*PI),v=Random(20,95)*strength,t=Random(.35f,.9f);
  particles.push_back({p,{std::cos(a)*v,std::sin(a)*v},Random(2,30),Random(25,105)*strength,t,t,c,Random(1,3.5f)});}
}
void TutorialGame::Blast(Vec2 p){
 Burst(p,45,{2.4f,.83f,.16f},2.2f);
 for(Enemy& e:enemies)if(e.hp>0&&Length(e.p-p)<205){e.hp-=e.kind==2?8:12;e.flash=.2f;
  if(e.kind==2)e.hp=std::max(1.f,e.hp);if(e.hp<=0)++kills;}
}
void TutorialGame::Damage(float amount){
 if(dodgeTime>0)return;float absorbed=std::min(armor,amount);armor-=absorbed;health-=amount-absorbed;damageDelay=0;injury=.8f;
 // The original prologue's final stand is a scripted, non-failing encounter.
 if(stage==LastStand){health=std::max(1.f,health);return;}
 if(health<=0){health=0;dead=true;ClearInput();}
}
void TutorialGame::Shoot(){
 if(gunCooldown>0||reload>0)return;
 if(magazine<=0){reload=1.5f;Notify(L"탄창 교체 중입니다.");return;}
 --magazine;gunCooldown=rifle?.17f:.32f;
 Vec2 aim=mouse-Project(player,40);Vec2 direction=Unit({aim.x/.8f+aim.y/.4f,aim.y/.4f-aim.x/.8f});
 if(Length(direction)<.01f)direction=facing;facing=direction;Vec2 end=player+direction*650;
 for(float d=16;d<650;d+=8)if(Blocked(player+direction*d,2)){end=player+direction*d;break;}
 float best=Length(end-player);Enemy* hit=nullptr;Barrel* barrel=nullptr;
 for(Enemy& e:enemies){if(e.hp<=0)continue;Vec2 rel=e.p-player;float along=rel.x*direction.x+rel.y*direction.y;
  if(along>0&&along<best&&SegmentDistance(e.p,player,end)<(e.kind==2?65:28)){best=along;hit=&e;}}
 for(Barrel& b:barrels){if(!b.alive)continue;Vec2 rel=b.p-player;float along=rel.x*direction.x+rel.y*direction.y;
  if(along>0&&along<best&&SegmentDistance(b.p,player,end)<25){best=along;barrel=&b;hit=nullptr;}}
 if(hit){bool strike=hit->stagger>0;hit->hp-=strike?20.f:1.f;hit->flash=.13f;end=hit->p;
  if(hit->kind==2)hit->hp=std::max(1.f,hit->hp);
  Burst(hit->p,7,{.33f,.075f,.09f});if(hit->hp<=0){++kills;armor=std::min(100.f,armor+(strike?20:3));if(strike){++gunStrikes;Notify(L"건 스트라이크 성공 · 장갑 회복");}}}
 if(barrel){barrel->alive=false;end=barrel->p;Blast(end);}
 shots.push_back({player,end,.10f});Burst(player,2,{1.8f,.95f,.28f},.3f);
}
void TutorialGame::Melee(){
 if(meleeCooldown>0)return;meleeCooldown=.65f;
 for(Enemy& e:enemies)if(e.hp>0&&Length(e.p-player)<120){e.hp-=e.kind==2?2:3;e.flash=.15f;Burst(e.p,7,{.36f,.09f,.10f});
  if(e.kind==2)e.hp=std::max(1.f,e.hp);if(e.hp<=0){++kills;armor=std::min(100.f,armor+6);}}
}
void TutorialGame::HeavyAttack(){
 if(meleeCooldown>0)return;meleeCooldown=1.0f;
 for(Enemy& e:enemies)if(e.hp>0&&Length(e.p-player)<145){e.stagger=4;e.flash=.2f;Move(e.p,Unit(e.p-player)*42,14);}
 Notify(L"강공격으로 적을 비틀거리게 했습니다. 표시된 적을 사격하십시오.");
}
int TutorialGame::NearbyEnemies(float radius)const{int n=0;for(const Enemy& e:enemies)if(e.hp>0&&Length(e.p-player)<radius)++n;return n;}
void TutorialGame::Advance(){
 ++stage;stageTime=hold=0;health=armor=100;
 const wchar_t* messages[]={L"",L"전우의 신호가 끊겼습니다. 추락 지점에서 폭탄을 회수하십시오.",L"생체 장애물을 해제하고 궤도 발사 시설로 진입하십시오.",L"시설 안에서 전우의 볼트 라이플을 확보하십시오.",L"볼트 라이플 확보. 폭탄을 발사 장치에 장전하십시오.",L"장전 완료. 승강기를 타고 활성화 제어기로 이동하십시오.",L"상층 도착. 제어기를 활성화하십시오.",L"기동 중입니다. 적의 습격을 버티십시오.",L"기동 완료. 적을 전부 잡을 필요는 없습니다. 제어기로 복귀하십시오.",L"폭탄 발사. 끝까지 싸우십시오.",L"타이투스의 이야기는 계속됩니다."};
 Notify(messages[stage]);
 if(stage==FindWeapon)RebuildNavigation();
 if(stage==LoadPayload){rifle=true;magazine=24;}
 if(stage==Activate){player=Console+Vec2(-180,180);camera=player;grenadesLeft=3;}
 if(stage==Activate)navigation.UpdateTarget(player);
 if(stage==Defend)waveTimer=0;
 if(stage==LastStand){launchTime=0;enemies.clear();waveTimer=0;}
 if(stage==Ending){complete=true;ClearInput();}
}
void TutorialGame::Key(unsigned char raw,bool down){
 unsigned char key=raw>='A'&&raw<='Z'?raw+('a'-'A'):raw;bool pressed=down&&!keys[key];keys[key]=down;
 if(key=='f'&&!down&&heavyCharge>0){if(heavyCharge<.45f)Melee();heavyCharge=0;}
 if(!pressed)return;
 if(key==27){if(!intro&&!dead&&!complete){paused=!paused;ClearInput();}return;}
 if(intro&&(key==13||key=='e')){intro=false;ClearInput();return;}
 if((dead||complete)&&key=='r'){Reset();return;}if(intro||paused||dead||complete)return;
 if(key=='r'&&reload<=0&&magazine<(rifle?24:12))reload=1.5f;
 if(key=='c'&&parryCooldown<=0){parryTime=.38f;parryCooldown=.9f;}
 if(key=='g'&&grenadesLeft>0&&stage>=ReachLift){
  Vec2 aim=mouse-Project(player);Vec2 q={aim.x/.8f+aim.y/.4f,aim.y/.4f-aim.x/.8f};q=q*.5f;
  if(Length(q)>450)q=Unit(q)*450;grenades.push_back({player,player+q,0});--grenadesLeft;
 }
 if(key=='e')for(Enemy& e:enemies)if(e.kind==1&&e.hp>0&&e.hp<=4&&Length(e.p-player)<110){
  e.hp=0;++kills;armor=std::min(100.f,armor+35);Burst(e.p,24,{.38f,.055f,.065f});Notify(L"처형 · 장갑 회복");break;}
 if(key==' '&&dodgeCooldown<=0){Vec2 input(float(keys['d'])-float(keys['a']),float(keys['s'])-float(keys['w']));
  dodgeDirection=Length(input)>0?Unit({input.x+input.y,input.y-input.x}):facing;dodgeTime=.24f;dodgeCooldown=1.3f;}
}
void TutorialGame::Mouse(int button,bool down){
 if(button==0)firing=down&&!intro&&!paused&&!dead&&!complete;
 if(button==2&&down&&!intro&&!paused&&!dead&&!complete)Melee();
}
void TutorialGame::Update(float dt){
 if(intro||paused||dead)return;if(complete){launchTime+=dt;return;}
 time+=dt;stageTime+=dt;toastTime=std::max(0.f,toastTime-dt);damageDelay+=dt;injury=std::max(0.f,injury-dt*2);
 gunCooldown-=dt;meleeCooldown-=dt;dodgeCooldown-=dt;parryTime-=dt;parryCooldown-=dt;
 if(reload>0){reload-=dt;if(reload<=0)magazine=rifle?24:12;}if(damageDelay>4)armor=std::min(100.f,armor+dt*13);
 if(keys['f']){float before=heavyCharge;heavyCharge+=dt;if(before<.45f&&heavyCharge>=.45f)HeavyAttack();}
 Vec2 input(float(keys['d'])-float(keys['a']),float(keys['s'])-float(keys['w']));Vec2 direction=Unit({input.x+input.y,input.y-input.x});
 moving=Length(direction)>0;
 if(dodgeTime>0){Move(player,dodgeDirection*(420*dt),20);dodgeTime-=dt;}
 else if(moving){Vec2 before=player;Move(player,direction*(105*dt),20);moving=Length(player-before)>.01f;
  if(moving){walk+=dt*9;footstep+=dt;if(footstep>.28f){Burst(player,4,{.26f,.31f,.20f,.6f},.2f);footstep=0;}}if(!firing)facing=direction;}
 camera=camera+(player-camera)*(1-std::exp(-dt*6));if(firing)Shoot();
 navigationTimer-=dt;if(navigationTimer<=0){navigation.UpdateTarget(player);navigationTimer=.25f;}
 for(Particle& p:particles){p.life-=dt;p.p=p.p+p.v*dt;p.z+=p.vz*dt;p.vz-=180*dt;if(p.z<0){p.z=0;p.vz=0;p.v=p.v*.9f;}}
 particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle& p){return p.life<=0;}),particles.end());
 for(Grenade& g:grenades){g.age+=dt;if(g.age>=.85f&&g.age-dt<.85f)Blast(g.end);}
 grenades.erase(std::remove_if(grenades.begin(),grenades.end(),[](const Grenade& g){return g.age>=.85f;}),grenades.end());
 for(Shot& s:shots)s.life-=dt;shots.erase(std::remove_if(shots.begin(),shots.end(),[](const Shot&s){return s.life<=0;}),shots.end());
 for(size_t i=0;i<enemies.size();++i){Enemy& e=enemies[i];e.flash=std::max(0.f,e.flash-dt);e.stagger=std::max(0.f,e.stagger-dt);if(e.hp<=0)continue;
  e.attack-=dt;Vec2 delta=player-e.p;float distance=Length(delta);
  if(e.warning){e.windup-=dt;if(e.windup<=0){
    if(distance<(e.kind==2?175:105)){
     if(parryTime>0&&e.kind!=2){e.stagger=3;e.hp-=2;armor=std::min(100.f,armor+12);Burst(e.p,12,{.4f,1.2f,1.8f});Notify(L"막아내기 성공 · 반격하십시오");if(e.hp<=0)++kills;}
     else Damage(e.kind==2?32:e.kind==1?18:10);
    }e.warning=false;e.attack=e.kind==2?2.3f:1.1f;}continue;}
  if(e.stagger>0)continue;
  float reach=e.kind==2?115.f:52.f;
  if(distance<1200&&distance>reach){Vec2 chase=Unit(navigation.Waypoint(e.p)-e.p),separation;
   for(size_t j=0;j<enemies.size();++j)if(i!=j&&enemies[j].hp>0){Vec2 q=e.p-enemies[j].p;float d=Length(q);if(d>0&&d<38)separation=separation+Unit(q)*(1-d/38);}
   Vec2 before=e.p;Move(e.p,Unit(chase+separation)*dt*(e.kind==2?95:74),14);
   if(Length(e.p-before)<dt*10)Move(e.p,Vec2(-chase.y,chase.x)*(dt*74),14);
  }
  if(distance<reach+28&&e.attack<=0){e.warning=true;e.windup=e.kind==2?.9f:.6f;}
 }
 if(dead)return;
 if(stage<=LoadPayload&&!encountered[stage]&&Length(player-Target())<540){encountered[stage]=true;Spawn(Target(),stage==Approach?5:stage==Recover?9:stage==PassGrowth?8:stage==FindWeapon?8:12,stage);
  Notify(stage==Approach?L"근접 공격을 배우십시오. 공격 키를 길게 누르면 강공격입니다.":L"파란 공격은 막아내기, 붉은 공격은 회피하십시오.");}
 if(stage==Approach&&encountered[0]&&Length(player-Beacon)<110&&NearbyEnemies(750)==0)Advance();
 if(stage==Defend){if(Length(player-Console)<650)defense+=dt;waveTimer-=dt;int active=0;for(const Enemy&e:enemies)if(e.hp>0)++active;
  if(waveTimer<=0&&defense<DefenseSeconds-6&&active<16&&Length(player-Console)<650){Spawn(Console,6,Defend);waveTimer=10;}
  if(defense>=DefenseSeconds)Advance();}
 if(stage==LastStand){launchTime+=dt;waveTimer-=dt;
  if(waveTimer<=0&&stageTime<18&&NearbyEnemies(1000)<12){Spawn(Console,5,LastStand);waveTimer=8;}
  if(stageTime>16&&!bossSpawned){bossSpawned=true;Enemy boss={Console+Vec2(250,-150),90,1,0,LastStand};boss.kind=2;enemies.push_back(boss);Notify(L"카니펙스 출현 · 붉은 공격을 회피하며 끝까지 저항하십시오.");}
  if(stageTime>42)Advance();}
 Interaction(dt);
 if(enemies.size()>100)enemies.erase(std::remove_if(enemies.begin(),enemies.end(),[](const Enemy&e){return e.hp<=0;}),enemies.end());
}
void TutorialGame::Interaction(float dt){
 if(stage==Approach||stage==Defend||stage>=LastStand){hold=0;return;}
 bool clear=true;
 if(stage<=LoadPayload)for(const Enemy&e:enemies)if(e.group==stage&&e.hp>0)clear=false;
 if(stage==ReachLift||stage==Activate)clear=NearbyEnemies(250)==0;
 // Once prepared, the original launch console remains usable during the ambush.
 if(Length(player-Target())<115&&clear&&keys['e']){hold+=dt;if(hold>=(stage==ReachLift?3.f:2.f))Advance();}else hold=0;
}
void TutorialGame::Box(Vec2 p,float sx,float sy,float height,Color c){
 Vec2 a=Project(p+Vec2(-sx,-sy)),b=Project(p+Vec2(sx,-sy)),d=Project(p+Vec2(-sx,sy)),e=Project(p+Vec2(sx,sy));Vec2 h(0,-height);
 Color base(c.r*3.2f,c.g*3.2f,c.b*3.2f,c.a);
 r.MaterialQuad(d,e,e+h,d+h,{0,0},{sx*2,0},{sx*2,height},{0,height},3,base.Shade(.63f));
 r.MaterialQuad(b,e,e+h,b+h,{0,0},{sy*2,0},{sy*2,height},{0,height},3,base.Shade(.83f));
 r.MaterialQuad(a+h,b+h,e+h,d+h,p+Vec2(-sx,-sy),p+Vec2(sx,-sy),p+Vec2(sx,sy),p+Vec2(-sx,sy),3,base);
 r.Line(d+h,e+h,1,{.45f,.49f,.41f,c.a});r.Line(b+h,e+h,1,{.42f,.45f,.39f,c.a});
}
void TutorialGame::Floor(){
 r.Rect(0,0,1280,800,{.085f,.13f,.10f});
 for(int y=0;y<60;++y)for(int x=0;x<60;++x){
  Vec2 p(x*100.f+50,y*100.f+50),s=Project(p);if(s.x<-150||s.x>1430||s.y<-100||s.y>900)continue;
  bool facility=p.y<4000&&p.x>4280&&p.x<5400;
  Vec2 a=p+Vec2(-50,-50),b=p+Vec2(50,-50),c=p+Vec2(50,50),d=p+Vec2(-50,50);
  r.MaterialQuad(Project(a),Project(b),Project(c),Project(d),a,b,c,d,facility?2:0,{1,1,1});
  unsigned hash=(x*73856093u)^(y*19349663u);float noise=(hash%100)/100.f;
  if(!facility&&hash%7==0){
   // Thin wet depressions have moving specular ripples rather than opaque blue disks.
   r.SoftEllipse(s,48+noise*25,17,{.055f,.15f,.145f,.60f});
   for(int k=0;k<3;++k){float wave=std::fmod(time*.6f+k*.32f+noise,1.f);
    r.Line(s+Vec2(-30+wave*15,k*4.f),s+Vec2(20-wave*7,k*4.f),1,{.36f,.53f,.46f,(1-wave)*.28f});}
  }
 }
 const Vec2 route[]={Start,Beacon,Wreck,Gate,Armory,Launcher,Lift,Console};
 for(int j=0;j<7;++j){Vec2 direction=Unit(route[j+1]-route[j]),normal(-direction.y,direction.x);float distance=Length(route[j+1]-route[j]);
  for(float d=0;d<distance;d+=90){Vec2 a=route[j]+direction*d,b=route[j]+direction*std::min(d+90,distance),s=Project(a);
   if(s.x<-230||s.x>1510||s.y<-170||s.y>1000)continue;
   for(int edge=0;edge<3;++edge){float width=edge==0?105:edge==1?91:77;Color tint(1.05f,1.f,.93f,edge==0?.12f:edge==1?.35f:1.f);
    Vec2 q0=a-normal*width,q1=b-normal*width,q2=b+normal*width,q3=a+normal*width;
    r.MaterialQuad(Project(q0),Project(q1),Project(q2),Project(q3),q0,q1,q2,q3,a.y<4100?2:1,tint);}
   if(int(d/90)%3==0){Vec2 lamp=Project(a+normal*95);r.SoftEllipse(lamp,22,12,{1.4f,.74f,.21f,.2f});r.Rect(lamp.x-2,lamp.y-3,4,3,{1.5f,1,.38f});}
  }
 }
 if(stage==Defend){for(int i=0;i<100;++i){float a=i*2*PI/100,b=(i+1)*2*PI/100;
  r.Line(Project(Console+Vec2(std::cos(a),std::sin(a))*650),Project(Console+Vec2(std::cos(b),std::sin(b))*650),2,{.7f,.55f,.22f,.4f});}}
 // Sealed organic passage: the cleared center aligns with the collision opening.
 for(int x=80;x<6000;x+=120){if(stage>=FindWeapon&&x>4350&&x<5050)continue;
  Vec2 p(float(x),4200),s=Project(p);if(s.x<-130||s.x>1410||s.y<-100||s.y>970)continue;
  if(x>4350&&x<5050){r.SoftEllipse(s,65,22,{.055f,.017f,.026f,.8f});
   for(int k=0;k<4;++k){r.Ellipse(s+Vec2(k*12.f-20,-20-k*8.f),17,35,{.26f,.16f,.20f});r.Line(s+Vec2(k*12.f-20,0),s+Vec2(k*12.f-24,-58),2,{.43f,.28f,.29f});}}
  else{r.Line(Project(p-Vec2(60,0)),Project(p+Vec2(60,0)),8,{.27f,.29f,.23f});r.Line(s,s+Vec2(0,-60),4,{.28f,.33f,.27f});}}
}
void TutorialGame::Shadows(){
 // Ground-only directional cast shadows plus local contact occlusion; never darken the HUD.
 for(const Prop& p:props){Vec2 s=Project(p.p);if(s.x<-250||s.x>1450||s.y<-160||s.y>950)continue;
  float length=p.height*.5f;
  if(environmentAtlas){int col=p.kind==0?int(p.p.x)%2:p.kind==1?2:0,row=p.kind==2?1:0;
   r.SpriteShadow(environmentAtlas,s,p.kind==0?130.f:100.f,length,col,row,4,2,.38f);}
  for(int j=0;j<5;++j){float t=j/4.f;r.SoftEllipse(s+Vec2(length*t,length*t*.36f),p.radius*(1.1f+t*.35f)+10,11+t*8,{.006f,.012f,.010f,.12f});}
  r.SoftEllipse(s,p.radius*1.4f,16,{.005f,.01f,.008f,.55f});
 }
 r.SoftEllipse(Project(player)+Vec2(20,8),52,18,{.003f,.006f,.007f,.42f});r.SoftEllipse(Project(player),25,9,{0,0,0,.65f});
 if(marineAtlas)r.SpriteShadow(marineAtlas,Project(player),90,73,moving?int(walk)%4:0,0,4,4,.42f);
 for(const Enemy&e:enemies)if(e.hp>0){Vec2 s=Project(e.p);if(s.x>-100&&s.x<1380&&s.y>-100&&s.y<900)r.SoftEllipse(s+Vec2(12,5),e.kind==2?95:35,e.kind==2?33:13,{0,.006f,.007f,.50f});}
 r.SoftEllipse(Project(Wreck)+Vec2(45,18),180,56,{.005f,.009f,.01f,.75f});
 r.SoftEllipse(Project(Launcher)+Vec2(40,10),160,48,{0,.007f,.008f,.58f});
}
void TutorialGame::Marine(Vec2 p,bool corpse){
    Vec2 s=Project(p);float bob=corpse?0:std::sin(walk)*1.4f;
    if(marineAtlas&&!corpse){
        Vec2 screenDirection={(facing.x-facing.y)*.8f,(facing.x+facing.y)*.4f};
        int row=screenDirection.y>=0?(screenDirection.x>=0?0:1):(screenDirection.x<0?2:3);
        int frame=moving?int(walk)%4:0;
        float breath=moving?0:std::sin(time*2.3f)*.55f;
        Vec2 size(142,142),position=s+Vec2(-71,-137+breath);
        if(dodgeTime>0){position.y+=6;size.y-=6;}
        r.Sprite(marineAtlas,position,size,frame,row,4,4,{1.18f,1.18f,1.12f});
        Vec2 direction=Unit(screenDirection),muzzle=s+Vec2(0,-56)+direction*33;
        if(gunCooldown>(rifle?.10f:.24f)){r.SoftEllipse(muzzle,27,20,{3.3f,1.5f,.35f,.7f});r.Ellipse(muzzle,5,4,{4,2.6f,.8f});}
        if(meleeCooldown>.3f){float angle=(.65f-meleeCooldown)*8;
            Vec2 tip=s+Vec2(std::cos(angle)*72,std::sin(angle)*28-42);r.Line(s+Vec2(-13,-43),tip,6,{.54f,.61f,.59f});
            for(int i=0;i<8;++i){float t=i/8.f;Vec2 tooth=s+Vec2(-13,-43)+(tip-(s+Vec2(-13,-43)))*t;r.Line(tooth,tooth+Vec2(3,-4),2,{.86f,.88f,.78f});}}
        if(parryTime>0)r.SoftEllipse(s+Vec2(0,-45),65,45,{.12f,.75f,1.7f,.25f});
        return;
    }
    if(corpse){r.Ellipse(s,25,10,{.12f,.13f,.14f});r.Rect(s.x-30,s.y-9,15,12,{.32f,.34f,.33f});return;}
    Vec2 aim=Unit(Vec2((facing.x-facing.y)*.8f,(facing.x+facing.y)*.4f));
    r.Rect(s.x-15,s.y-22+bob,12,22,{.09f,.11f,.12f});r.Rect(s.x+4,s.y-22-bob,12,22,{.085f,.095f,.10f});
    r.Rect(s.x-18,s.y-6+bob,16,7,{.19f,.21f,.21f});r.Rect(s.x+3,s.y-6-bob,17,7,{.19f,.21f,.21f});
    r.Rect(s.x-18,s.y-57+bob,36,34,{.11f,.13f,.14f});
    r.Rect(s.x-23,s.y-54+bob,8,23,{.24f,.26f,.25f});r.Rect(s.x+15,s.y-54+bob,8,23,{.20f,.22f,.22f});
    r.Ellipse(s+Vec2(-23,-41+bob),13,15,{.48f,.51f,.50f}); // Deathwatch silver arm.
    r.Ellipse(s+Vec2(23,-41+bob),14,15,{.095f,.11f,.12f});
    r.Line(s+Vec2(13,-49+bob),s+Vec2(30,-49+bob),2,{.34f,.34f,.29f});
    r.Rect(s.x-14,s.y-45+bob,28,22,{.16f,.18f,.18f});
    r.Line(s+Vec2(-11,-36+bob),s+Vec2(11,-36+bob),3,{.54f,.47f,.31f});
    r.Line(s+Vec2(0,-40+bob),s+Vec2(0,-30+bob),3,{.66f,.60f,.44f});
    r.Ellipse(s+Vec2(0,-59+bob),11,13,{.25f,.27f,.26f});
    r.Rect(s.x-8,s.y-61+bob,16,3,{.85f,.12f,.065f});
    r.Rect(s.x-6,s.y-56+bob,12,7,{.105f,.12f,.12f});
    Vec2 hand=s+Vec2(13,-27+bob),barrel=hand+aim*31;
    r.Line(hand,barrel,10,{.10f,.12f,.12f});r.Line(hand+Vec2(0,-4),barrel+Vec2(0,-4),2,{.45f,.44f,.35f});
    if(gunCooldown>.12f){r.Ellipse(barrel,12,8,{1,.66f,.24f,.8f});r.Ellipse(barrel,5,5,{1,.95f,.64f});}
    if(meleeCooldown>.58f){float a=(.85f-meleeCooldown)*12;
        Vec2 tip=s+Vec2(std::cos(a)*65,std::sin(a)*25-25);
        r.Line(s+Vec2(-18,-28),tip,5,{.7f,.72f,.62f});r.Ellipse(s+Vec2(0,-20),72,29,{.72f,.75f,.51f,.09f});}
    if(stage>=PassGrowth && stage<=LoadPayload){r.Rect(s.x-13,s.y-48,10,25,{.34f,.43f,.19f});r.Rect(s.x-11,s.y-45,6,5,Gold);}
}
void TutorialGame::Tyranid(const Enemy& e){
    Vec2 s=Project(e.p);bool alive=e.hp>0;
    if(alive){
        if(e.warning){Color warning=e.kind==2?Color(2,.10f,.035f):Color(.10f,.75f,2);
            r.SoftEllipse(s,60+(1-e.windup)*20,25,Color(warning.r,warning.g,warning.b,.35f));
            r.Ellipse(s+Vec2(0,e.kind==2?-165.f:-85.f),6,6,warning);}
        if(e.stagger>0){Vec2 mark=s+Vec2(0,-80);r.Line(mark+Vec2(-7,-7),mark+Vec2(7,7),2,Red);r.Line(mark+Vec2(7,-7),mark+Vec2(-7,7),2,Red);}
    }
    if(enemyAtlas){
        float size=e.kind==2?235.f:e.kind==1?130.f:90.f;
        int frame=alive&&!e.warning&&e.stagger<=0?int(time*(e.kind==2?5:9)+e.p.x*.005f)%4:0;
        Color tint=e.flash>0?Color(1.8f,1.8f,1.8f):Color(1.05f,1.03f,.98f);
        if(!alive){tint={.28f,.16f,.15f,.68f};r.Sprite(enemyAtlas,s+Vec2(-size*.5f,-size*.22f),{size,size*.28f},0,e.kind,4,3,tint);}
        else {bool flip=Project(player).x>s.x;r.Sprite(enemyAtlas,s+Vec2(flip?size*.5f:-size*.5f,-size*.93f),{flip?-size:size,size},frame,e.kind,4,3,tint);}
        return;
    }
    r.Ellipse(s,25,9,alive?Color(0,0,0,.4f):Color(.20f,.055f,.06f,.75f));
    if(!alive){r.Ellipse(s,17,6,{.19f,.12f,.17f});return;}
    float wobble=std::sin(time*13+e.p.x)*3;
    Color shell=e.flash>0?White:Color(.29f,.18f,.30f);
    for(int side:{-1,1}){
        r.Line(s+Vec2(side*8.f,-13),s+Vec2(side*30.f,-9+wobble),3,{.62f,.57f,.40f});
        r.Line(s+Vec2(side*30.f,-9+wobble),s+Vec2(side*23.f,2),2,{.71f,.64f,.43f});
        r.Triangle(s+Vec2(side*13.f,-22),s+Vec2(side*39.f,-36),s+Vec2(side*24.f,-8),{.65f,.59f,.43f});
    }
    r.Ellipse(s+Vec2(0,-15),13,18,shell);
    for(int i=0;i<3;++i)r.Line(s+Vec2(-10,-11-i*7.f),s+Vec2(9,-14-i*7.f),2,shell.Shade(1.4f));
    r.Triangle(s+Vec2(-11,-27),s+Vec2(12,-27),s+Vec2(0,-43),{.42f,.30f,.40f});
    r.Rect(s.x-6,s.y-31,3,2,{1,.57f,.16f});r.Rect(s.x+3,s.y-31,3,2,{1,.57f,.16f});
}
void TutorialGame::DrawProp(const Prop& prop){
    Vec2 s=Project(prop.p);if(s.x<-220||s.x>1500||s.y<-100||s.y>1100)return;
    float alpha=Length(prop.p-player)<180 && prop.p.x+prop.p.y>player.x+player.y?.28f:1.f;
    if(environmentAtlas){
        int col=prop.kind==0?(int(prop.p.x)%2):prop.kind==1?2:0,row=prop.kind==2?1:0;
        float height=prop.kind==0?prop.height+95:prop.kind==1?135:235;
        float sway=prop.kind==0?std::sin(time*1.15f+prop.p.y*.01f)*2.2f:0;
        r.Sprite(environmentAtlas,s+Vec2(-height*.5f+sway,-height*.89f),{height,height},col,row,4,2,{.91f,.98f,.90f,alpha});
        if(prop.kind==2){Vec2 light=s+Vec2(0,-85);r.SoftEllipse(light,33,28,{1.7f,.64f,.11f,.18f});r.Ellipse(light,2,4,{2,1,.22f});}
        return;
    }
    if(prop.kind==0){
        r.Ellipse(s+Vec2(30,3),40,12,{0,0,0,.23f});
        r.Line(s,s+Vec2(-8,-prop.height),13,{.075f,.09f,.07f,alpha});
        for(int j=0;j<5;++j){float side=j%2?1.f:-1.f;float y=-prop.height+j*19;
            r.Triangle(s+Vec2(-8,y-40),s+Vec2(side*(55-j*6.f),y+26),s+Vec2(0,y+9),{.08f+j*.008f,.15f+j*.009f,.12f,alpha});}
        r.Line(s+Vec2(-8,-prop.height),s+Vec2(26,-prop.height+55),2,{.18f,.24f,.13f,alpha});
    }else if(prop.kind==1){Box(prop.p,prop.radius,prop.radius*.65f,prop.height*.3f,{.22f,.25f,.22f,alpha});}
    else{
        Box(prop.p,prop.radius,prop.radius,20,{.23f,.25f,.22f,alpha});
        Box(prop.p,prop.radius*.60f,prop.radius*.60f,prop.height,{.28f,.30f,.27f,alpha});
        Vec2 top=Project(prop.p,prop.height);
        r.Triangle(top+Vec2(-30,0),top+Vec2(30,0),top+Vec2(0,-55),{.25f,.27f,.25f,alpha});
        r.Line(s+Vec2(0,-35),s+Vec2(0,-prop.height+15),4,{.055f,.075f,.072f,alpha});
        r.Ellipse(s+Vec2(0,-prop.height+20),5,8,{.75f,.28f,.08f,alpha});
    }
}
void TutorialGame::World(){
 Floor();Shadows();
 struct DrawItem{float depth;int kind;size_t index;};std::vector<DrawItem> items;
 for(size_t i=0;i<props.size();++i)items.push_back({props[i].p.x+props[i].p.y,0,i});
 for(size_t i=0;i<enemies.size();++i)items.push_back({enemies[i].p.x+enemies[i].p.y,1,i});
 for(size_t i=0;i<barrels.size();++i)if(barrels[i].alive)items.push_back({barrels[i].p.x+barrels[i].p.y,6,i});
 items.push_back({player.x+player.y,2,0});items.push_back({Wreck.x+Wreck.y,3,0});
 items.push_back({Gate.x+Gate.y,4,0});items.push_back({Launcher.x+Launcher.y,5,0});
 items.push_back({Armory.x+Armory.y,7,0});items.push_back({Lift.x+Lift.y,8,0});items.push_back({Console.x+Console.y,9,0});
 std::stable_sort(items.begin(),items.end(),[](const DrawItem&a,const DrawItem&b){return a.depth<b.depth;});
 for(const DrawItem& item:items){
  if(item.kind==0)DrawProp(props[item.index]);
  if(item.kind==1){Vec2 s=Project(enemies[item.index].p);if(s.x>-170&&s.x<1450&&s.y>-150&&s.y<1050)Tyranid(enemies[item.index]);}
  if(item.kind==2)Marine(player);
  if(item.kind==3){Vec2 s=Project(Wreck);if(s.x>-400&&s.x<1680&&s.y>-150&&s.y<1100){
   Box(Wreck+Vec2(50,0),145,42,40,{.16f,.19f,.19f});
   Box(Wreck+Vec2(80,0),40,130,22,{.19f,.22f,.22f});Box(Wreck+Vec2(-10,-10),50,36,72,{.17f,.20f,.20f});
   for(int side:{-1,1}){Vec2 engine=Project(Wreck+Vec2(100,side*105.f),35);
    r.Ellipse(engine,25,15,{.07f,.095f,.10f});r.Ellipse(engine,17,10,{.17f,.20f,.20f});r.Ellipse(engine,10,7,{.022f,.034f,.036f});
    r.Line(engine+Vec2(-18,-10),engine+Vec2(18,-10),2,{.39f,.44f,.41f});}
   r.Quad(s+Vec2(-54,-84),s+Vec2(-12,-91),s+Vec2(-6,-70),s+Vec2(-49,-62),{.075f,.22f,.23f});
   for(int i=0;i<6;++i)r.Line(s+Vec2(-42+i*13.f,-39),s+Vec2(-35+i*13.f,-50),2,{.40f,.35f,.24f});
   Marine(Wreck+Vec2(-60,65),true);if(stage<=Recover)Box(Wreck+Vec2(-20,15),14,22,20,{.43f,.48f,.22f});
  }}
  if(item.kind==4){Vec2 s=Project(Gate);if(s.x>-150&&s.x<1430&&s.y>-100&&s.y<1000){
   if(environmentAtlas)r.Sprite(environmentAtlas,s+Vec2(-75,-128),{150,150},3,1,4,2,{.85f,.8f,.9f});
   else r.Ellipse(s+Vec2(0,-25),24,35,{.27f,.13f,.20f});
   r.SoftEllipse(s+Vec2(0,-45),40,35,stage>=FindWeapon?Color(.16f,.6f,.32f,.2f):Color(.8f,.20f,.34f,.3f));
  }}
  if(item.kind==5){Vec2 s=Project(Launcher);if(s.x>-350&&s.x<1630&&s.y>-250&&s.y<1150){
   Box(Launcher+Vec2(0,-100),140,140,20,{.29f,.31f,.28f});Box(Launcher+Vec2(0,-100),70,70,55,{.22f,.27f,.26f});
   Vec2 rocket=Project(Launcher+Vec2(0,-100));float rise=stage>=LastStand?std::min(1400.f,launchTime*launchTime*75):0;
   r.Rect(rocket.x-22,rocket.y-210-rise,44,156,{.49f,.52f,.45f});
   r.Rect(rocket.x-18,rocket.y-210-rise,8,156,{.64f,.66f,.56f});r.Rect(rocket.x+12,rocket.y-210-rise,9,156,{.25f,.30f,.27f});
   r.Triangle(rocket+Vec2(-22,-210-rise),rocket+Vec2(22,-210-rise),rocket+Vec2(0,-256-rise),{.63f,.66f,.55f});
   for(int i=0;i<4;++i)r.Rect(rocket.x-23,rocket.y-82-i*35-rise,46,4,{.21f,.25f,.22f});
   for(int side:{-1,1}){Vec2 rail=rocket+Vec2(side*57.f,0);r.Line(rail,rail+Vec2(0,-235),6,{.24f,.30f,.28f});
    for(int i=0;i<6;++i)r.Line(rail+Vec2(-11,-i*35.f),rail+Vec2(11,-i*35.f),3,{.34f,.40f,.34f});}
   if(stage>=LastStand){r.SoftEllipse(rocket+Vec2(0,-40-rise),85,110,{3.7f,1.9f,.35f,.65f});r.Triangle(rocket+Vec2(-18,-54-rise),rocket+Vec2(18,-54-rise),rocket+Vec2(0,65-rise),{4,2,.3f});}
   Box(Launcher+Vec2(40,30),26,26,42,{.24f,.30f,.27f});
   r.Rect(s.x+2,s.y-20,25,10,stage>=ReachLift?Color(.2f,1.6f,.65f):Color(1.3f,.71f,.18f));
  }}
  if(item.kind==6){Vec2 s=Project(barrels[item.index].p);if(s.x>-90&&s.x<1370&&s.y>-90&&s.y<950){
   Box(barrels[item.index].p,17,17,39,{.42f,.16f,.08f});r.Rect(s.x-12,s.y-26,24,6,{.77f,.58f,.23f});
   r.Line(s+Vec2(-7,-29),s+Vec2(8,-20),3,{.05f,.06f,.05f});}}
  if(item.kind==7){Vec2 s=Project(Armory);if(s.x>-180&&s.x<1460&&s.y>-100&&s.y<1000){
   Marine(Armory+Vec2(35,15),true);
   if(environmentAtlas)r.Sprite(environmentAtlas,s+Vec2(-85,-82),{125,125},1,1,4,2);
   if(!rifle){r.Line(s+Vec2(-10,-20),s+Vec2(36,-32),10,{.26f,.32f,.29f});r.SoftEllipse(s+Vec2(10,-20),45,20,{.8f,.71f,.35f,.2f});}}}
  if(item.kind==8){Vec2 s=Project(Lift);if(s.x>-230&&s.x<1510&&s.y>-200&&s.y<1050){
   Box(Lift,90,90,10,{.27f,.31f,.28f});
   for(int i=-3;i<=3;++i)r.Line(Project(Lift+Vec2(-80,i*23.f),12),Project(Lift+Vec2(80,i*23.f),12),2,{.10f,.14f,.13f});
   for(int side:{-1,1})r.Line(Project(Lift+Vec2(side*90.f,-90)),Project(Lift+Vec2(side*90.f,-90),180),6,{.32f,.36f,.29f});}}
  if(item.kind==9){Vec2 s=Project(Console);if(s.x>-220&&s.x<1500&&s.y>-200&&s.y<1050){
   Box(Console+Vec2(0,-30),95,65,12,{.28f,.32f,.28f});Box(Console+Vec2(25,-20),40,30,60,{.21f,.27f,.24f});
   r.Quad(s+Vec2(-5,-80),s+Vec2(50,-68),s+Vec2(50,-40),s+Vec2(-5,-52),{.075f,.40f,.24f});
   for(int i=0;i<5;++i)r.Line(s+Vec2(1,-70+i*4.f),s+Vec2(38-i*4.f,-62+i*4.f),1,{.43f,1.35f,.67f});
   r.SoftEllipse(s+Vec2(18,-55),80,60,{.12f,.74f,.38f,.17f});
   if(environmentAtlas)r.Sprite(environmentAtlas,s+Vec2(-150,-85),{125,125},1,1,4,2);
  }}
 }
 DrawEffects();
}
void TutorialGame::DrawEffects(){
 for(const Shot& shot:shots){r.Line(Project(shot.a,52),Project(shot.b,32),2,{2.8f,1.9f,.7f,.95f});r.SoftEllipse(Project(shot.b,32),16,14,{2.1f,1.3f,.4f,.6f});}
 for(const Particle& p:particles){Vec2 s=Project(p.p,p.z);float alpha=p.life/p.total;
  Color c=p.color;c.a*=alpha;r.Ellipse(s,p.size,p.size*.7f,c);if(c.r>1)r.SoftEllipse(s,p.size*4,p.size*3,{c.r,c.g,c.b,alpha*.15f});}
 for(const Grenade& g:grenades){float t=Clamp(g.age/.85f,0,1);Vec2 p=g.start+(g.end-g.start)*t;
  r.SoftEllipse(Project(p),12,6,{0,0,0,.4f});Vec2 s=Project(p,std::sin(t*PI)*100);r.Ellipse(s,4,5,{.38f,.44f,.25f});}
 // World-anchored fire, smoke and steam. Camera movement does not drag their sources.
 Vec2 fire=Project(Wreck+Vec2(100,-30));
 if(fire.x>-200&&fire.x<1480&&fire.y>-100&&fire.y<1000){
  r.SoftEllipse(fire,150,95,{2.5f,.88f,.12f,.16f});
  for(int i=0;i<15;++i){float age=std::fmod(time*.65f+i*.173f,1.f);float dx=std::sin(i*3.7f+time)*15;
   r.SoftEllipse(fire+Vec2(dx+age*35,-age*190),18+age*60,25+age*45,{.14f,.16f,.15f,(1-age)*.13f});}
  for(int i=0;i<9;++i){float flame=std::sin(time*8+i)*7;Vec2 f=fire+Vec2(i*5.f-20,0);
   r.Triangle(f+Vec2(-8,0),f+Vec2(8,0),f+Vec2(3,-35-flame),{2.1f,.72f,.12f,.8f});}
 }
 for(int i=0;i<8;++i){float x=std::fmod(i*211.f+time*(5+i%3),1500.f)-100;
  r.SoftEllipse({x,190+float((i*131)%550)},250,45,{.42f,.50f,.42f,.055f});}
 // Restrained rain and ground splash rings.
 for(int i=0;i<75;++i){float x=std::fmod(i*127.f+time*70,1300.f),y=std::fmod(i*193.f+time*410,850.f);
  r.Line({x,y},{x-4,y+11},1,{.64f,.73f,.68f,.13f});}
 for(int i=0;i<22;++i){float age=std::fmod(time*.8f+i*.137f,1.f);Vec2 p={float((i*137)%1250),float(270+(i*93)%390)};
  r.SoftEllipse(p,3+age*13,1+age*4,{.64f,.72f,.62f,(1-age)*.12f});}
 // Canopy light breaks: broad low-opacity shafts, composited before bloom.
 for(int i=0;i<4;++i){float x=130+i*350.f+std::sin(time*.08f)*25;
  r.Quad({x-150,0},{x-105,0},{x+240,680},{x+90,680},{.64f,.72f,.47f,.025f});}
}
void TutorialGame::Hud(){
 r.Rect(28,24,430,98,{.016f,.027f,.026f,.92f});r.Rect(28,24,3,98,Gold);
 r.Text(46,34,L"카다쿠 · 데스워치",Gold,.85f);r.Text(46,61,L"추락한 전우들",White,1.2f);
 r.Text(46,97,L"타이투스의 여정 · 궤도 발사 시설",Muted,.66f);
 const wchar_t* titles[]={L"추락 지점으로 전진",L"전우에게서 바이러스 폭탄 회수",L"생체 장애물 해제 · 시설 진입",L"전우의 볼트 라이플 확보",L"발사 장치에 폭탄 장전",L"승강기로 활성화 제어기에 접근",L"궤도 발사 장치 활성화",L"기동이 완료될 때까지 생존",L"바이러스 폭탄 발사",L"마지막 저항",L"이야기는 계속된다"};
 const wchar_t* details[]={L"이동 후 적을 처치하십시오. 강공격 뒤 사격하면 장갑이 회복됩니다.",L"전우 곁의 적을 처치하고 상호작용 키를 길게 누르십시오.",L"워리어의 파란 공격을 막고 생체 장치를 작동하십시오.",L"시설 안의 전우에게 접근해 주무기를 확보하십시오.",L"발사 장치 주변을 확보한 뒤 폭탄을 장전하십시오.",L"승강기에서 상호작용 키를 길게 누르십시오.",L"상층 제어기에서 발사 준비를 시작하십시오.",L"표시된 영역을 지키십시오. 수류탄과 폭발물을 활용하십시오.",L"남은 적을 전부 잡을 필요는 없습니다. 제어기를 작동하십시오.",L"끝까지 저항하십시오. 붉은 공격은 회피해야 합니다.",L"타이투스의 이후 이야기는 다음 구간으로 이어집니다."};
 r.Rect(28,139,520,82,{.018f,.029f,.027f,.9f});r.Text(44,147,titles[stage],Gold,.86f);r.Text(44,182,details[stage],White,.60f);
 r.Rect(1050,24,202,192,{.016f,.026f,.026f,.94f});r.Text(1063,30,L"작전 구역",Muted,.7f);
 for(int i=0;i<5;++i){r.Line({1066+i*42.f,64},{1066+i*42.f,202},1,{.13f,.19f,.17f});r.Line({1066,64+i*34.f},{1234,64+i*34.f},1,{.13f,.19f,.17f});}
 auto mapPoint=[](Vec2 p){return Vec2(1066+p.x/6000*168,64+p.y/6000*136);};
 Vec2 path[]={Start,Beacon,Wreck,Gate,Armory,Launcher,Lift,Console};for(int i=0;i<7;++i)r.Line(mapPoint(path[i]),mapPoint(path[i+1]),1,{.42f,.37f,.23f});
 for(const Enemy&e:enemies)if(e.hp>0)r.Ellipse(mapPoint(e.p),e.kind==2?3:1.5f,e.kind==2?3:1.5f,Red);
 r.Ellipse(mapPoint(Target()),4,4,Gold);r.Ellipse(mapPoint(player),3,3,White);
 std::wstring elapsed=std::to_wstring(int(time)/60)+L"분 "+std::to_wstring(int(time)%60)+L"초";r.Text(930,36,elapsed,Muted,.68f);
 if(!complete&&stage!=LastStand){Vec2 t=Project(Target(),15);Vec2 marker(Clamp(t.x,565,1010),Clamp(t.y,255,615));
  if(t.x>40&&t.x<1240&&t.y>235&&t.y<650)marker=t;float pulse=2*std::sin(time*3);
  r.Line(marker+Vec2(-9,0),marker+Vec2(0,-9-pulse),2,Gold);r.Line(marker+Vec2(0,-9-pulse),marker+Vec2(9,0),2,Gold);
  r.Line(marker+Vec2(-9,0),marker+Vec2(0,9+pulse),2,Gold);r.Line(marker+Vec2(0,9+pulse),marker+Vec2(9,0),2,Gold);
  if(Length(t-marker)>30)r.Line(marker,marker+Unit(t-marker)*25,2,Gold);
 }
 if(stage==Defend){r.Rect(408,621,465,50,{.018f,.03f,.027f,.94f});r.Rect(424,660,432,3,{.18f,.22f,.19f});r.Rect(424,660,432*Clamp(defense/DefenseSeconds,0,1),3,Gold);
  std::wstring label=L"기동 완료까지 "+std::to_wstring(std::max(0,int(DefenseSeconds-defense)))+L"초";
  if(Length(player-Console)>=650)label=L"작전 구역으로 복귀하십시오 · 진행 일시정지";r.Text(432,626,label,Gold,.78f);}
 if(stage>Approach&&stage!=Defend&&stage<LastStand&&Length(player-Target())<135){
  bool clear=true;if(stage<=LoadPayload)for(const Enemy&e:enemies)if(e.hp>0&&e.group==stage)clear=false;
  if(stage==ReachLift||stage==Activate)clear=NearbyEnemies(250)==0;
  r.Rect(430,570,425,45,{.015f,.025f,.025f,.95f});r.Text(446,578,clear?L"[ E 길게 ] 상호작용":L"해당 구역의 적을 먼저 처리하십시오",clear?Gold:Red,.82f);
  r.Rect(430,614,425*Clamp(hold/(stage==ReachLift?3.f:2.f),0,1),3,Gold);
 }
 for(const Enemy&e:enemies)if(e.hp>0&&e.kind==1&&e.hp<=4&&Length(e.p-player)<110){r.Text(558,536,L"[ E ] 처형 · 장갑 회복",Gold,.75f);break;}
 if(stage==LastStand&&bossSpawned){r.Rect(460,237,360,40,{.035f,.018f,.016f,.9f});r.Text(475,243,L"카니펙스 · 마지막 저항",Red,.85f);}
 r.Rect(28,698,1224,78,{.012f,.022f,.023f,.96f});r.Text(44,705,L"타이투스",Gold,.78f);
 r.Text(44,742,L"생명",Muted,.63f);r.Rect(87,752,140,5,{.2f,.12f,.1f});r.Rect(87,752,140*health/100,5,Red);
 r.Text(245,742,L"장갑",Muted,.63f);r.Rect(288,752,140,5,{.11f,.17f,.18f});r.Rect(288,752,140*armor/100,5,{.42f,.61f,.62f});
 std::wstring weapon=reload>0?L"탄창 교체 중":(rifle?L"볼트 라이플  ":L"볼트 피스톨  ")+std::to_wstring(magazine)+L"발";
 r.Text(460,704,weapon,White,.76f);r.Text(460,742,L"수류탄 "+std::to_wstring(grenadesLeft)+L"개 · 재장전 [R]",Muted,.63f);
 r.Text(760,704,L"이동 [W A S D]  사격 [좌클릭]  근접 [F]",White,.66f);
 r.Text(760,741,L"막기 [C]  회피 [스페이스]  수류탄 [G]  메뉴 [이스케이프]",Muted,.58f);
 if(toastTime>0){r.Rect(286,280,708,40,{.02f,.035f,.03f,.94f});r.Text(302,286,toast,White,.67f);}
 r.Line(mouse+Vec2(-8,0),mouse+Vec2(-3,0),1,Gold);r.Line(mouse+Vec2(3,0),mouse+Vec2(8,0),1,Gold);
 r.Line(mouse+Vec2(0,-8),mouse+Vec2(0,-3),1,Gold);r.Line(mouse+Vec2(0,3),mouse+Vec2(0,8),1,Gold);
}
void TutorialGame::Draw(){
 r.Begin();r.SetTime(time);World();r.Composite(time,injury);Hud();
 if(intro||paused||dead||complete){
  r.Rect(0,0,1280,800,{.006f,.012f,.014f,.74f});r.Rect(254,165,772,470,{.025f,.039f,.035f,.97f});r.Rect(254,165,772,3,Gold);
  r.Text(294,193,L"데스워치 · 작전 기록",Gold,.88f);
  if(intro){
   r.Text(294,237,L"카다쿠, 추락 이후",White,1.5f);
   r.Text(294,299,L"타이투스는 외계 위협에 맞서는 데스워치의 전사입니다.",White,.83f);
   r.Text(294,334,L"수송기는 추락했고 전우들의 신호가 끊겼습니다.",Muted,.84f);
   r.Text(294,369,L"바이러스 폭탄을 회수해 발사하십시오. 침공을 늦춰야 합니다.",Muted,.80f);
   r.Text(294,422,L"이동 [W A S D] · 근접 [F] · 강공격 [F 길게] · 사격 [좌클릭]",Gold,.71f);
   r.Text(294,455,L"막아내기 [C] · 회피 [스페이스] · 처형 및 상호작용 [E]",Gold,.74f);
   r.Text(294,507,L"[ 엔터 ] 임무 시작",White,1.0f);
   r.Text(294,566,L"약 5분 목표 · 원작의 사건 순서를 따른 쿼터뷰 재구성",Muted,.69f);
  }else if(dead){
   r.Text(294,255,L"작전 수행 불가",Red,1.55f);
   r.Text(294,330,L"파란 예고는 막아내고, 붉은 예고는 회피하십시오.",White,.87f);
   r.Text(294,370,L"강공격 후 사격과 처형으로 장갑을 회복할 수 있습니다.",Muted,.81f);
   r.Text(294,515,L"[ R ] 처음부터 다시 시작",Gold,.95f);
  }else if(complete){
   r.Text(294,251,L"임무는 완수되었다",White,1.45f);
   r.Text(294,319,L"바이러스 폭탄은 발사되었지만, 타이투스는 치명상을 입습니다.",White,.80f);
   r.Text(294,356,L"그의 이야기는 울트라마린의 구출 이후로 이어집니다.",Muted,.83f);
   r.Text(294,410,L"소요 시간 "+std::to_wstring(int(time)/60)+L"분 "+std::to_wstring(int(time)%60)+L"초 · 처치 "+std::to_wstring(kills),Gold,.85f);
   r.Text(294,445,L"건 스트라이크 "+std::to_wstring(gunStrikes)+L"회",Muted,.75f);
   r.Text(294,515,L"[ R ] 다시 플레이",Gold,.95f);
   r.Text(294,574,L"원작 영상 대신 요약 연출로 마무리한 프로토타입입니다.",Muted,.66f);
  }else{
   r.Text(294,251,L"일시정지",White,1.55f);
   r.Text(294,325,L"근접 [F] · 강공격 [F 길게] · 막기 [C] · 사격 [좌클릭]",White,.78f);
   r.Text(294,365,L"처형·상호작용 [E] · 회피 [스페이스] · 재장전 [R]",Muted,.79f);
   r.Text(294,405,L"수류탄 [G] · 폭발물을 사격하면 주변 적을 공격합니다.",Muted,.78f);
   r.Text(294,515,L"[ 이스케이프 ] 계속",Gold,.95f);
  }
 }
 r.End();
}
