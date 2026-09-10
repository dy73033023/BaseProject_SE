#include "stdafx.h"
#include "NavigationGrid.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <utility>

namespace { const int Unreachable=100000000; }
void NavigationGrid::Reset(int cells,float cellSize){
    width=std::max(1,cells);size=std::max(1.f,cellSize);targetCell=-1;
    blocked.assign(width*width,0);distance.assign(width*width,Unreachable);
    for(int i=0;i<width;++i){blocked[i]=blocked[(width-1)*width+i]=1;blocked[i*width]=blocked[i*width+width-1]=1;}
}
bool NavigationGrid::Walkable(int x,int y)const{return x>=0&&y>=0&&x<width&&y<width&&!blocked[y*width+x];}
int NavigationGrid::Cell(Vec2 p)const{
    if(p.x<0||p.y<0||p.x>=width*size||p.y>=width*size)return -1;
    return int(p.y/size)*width+int(p.x/size);
}
Vec2 NavigationGrid::Center(int i)const{return {(i%width+.5f)*size,(i/width+.5f)*size};}
void NavigationGrid::BlockCircle(Vec2 p,float radius){
    radius+=size*.707107f;
    int x0=std::max(0,int((p.x-radius)/size)),x1=std::min(width-1,int((p.x+radius)/size));
    int y0=std::max(0,int((p.y-radius)/size)),y1=std::min(width-1,int((p.y+radius)/size));
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){Vec2 delta=Center(y*width+x)-p;
        if(delta.x*delta.x+delta.y*delta.y<=radius*radius)blocked[y*width+x]=1;}
    targetCell=-1;
}
void NavigationGrid::BlockRectangle(Vec2 minimum,Vec2 maximum){
    int x0=std::max(0,int(minimum.x/size)),x1=std::min(width-1,int(maximum.x/size));
    int y0=std::max(0,int(minimum.y/size)),y1=std::min(width-1,int(maximum.y/size));
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)blocked[y*width+x]=1;
    targetCell=-1;
}
void NavigationGrid::UpdateTarget(Vec2 p){
    goal=p;int target=Cell(p);if(target<0)return;
    if(blocked[target]){
        int best=-1;float bestDistance=1e20f,cx=float(target%width),cy=float(target/width);
        for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx){int x=int(cx)+dx,y=int(cy)+dy;if(!Walkable(x,y))continue;
            Vec2 d=Center(y*width+x)-p;float value=d.x*d.x+d.y*d.y;
            if(value<bestDistance){bestDistance=value;best=y*width+x;}}
        if(best<0)return;target=best;
    }
    if(target==targetCell)return;targetCell=target;
    std::fill(distance.begin(),distance.end(),Unreachable);
    using Node=std::pair<int,int>;
    std::priority_queue<Node,std::vector<Node>,std::greater<Node>> queue;
    distance[target]=0;queue.push({0,target});
    while(!queue.empty()){
        Node current=queue.top();queue.pop();if(current.first!=distance[current.second])continue;
        int x=current.second%width,y=current.second/width;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
            if((!dx&&!dy)||!Walkable(x+dx,y+dy))continue;
            if(dx&&dy&&(!Walkable(x+dx,y)||!Walkable(x,y+dy)))continue;
            int next=(y+dy)*width+x+dx,cost=current.first+(dx&&dy?14:10);
            if(cost<distance[next]){distance[next]=cost;queue.push({cost,next});}
        }
    }
}
bool NavigationGrid::ClearLine(Vec2 a,Vec2 b)const{
    Vec2 d=b-a;float length=std::sqrt(d.x*d.x+d.y*d.y);int samples=std::max(1,int(length/(size*.2f))+1);
    int previous=-1;
    for(int i=0;i<=samples;++i){int cell=Cell(a+d*(float(i)/samples));if(cell<0||blocked[cell])return false;
        if(previous>=0){int x=previous%width,y=previous/width,nx=cell%width,ny=cell/width;
            if(x!=nx&&y!=ny&&(!Walkable(nx,y)||!Walkable(x,ny)))return false;}
        previous=cell;
    }return true;
}
Vec2 NavigationGrid::Waypoint(Vec2 p)const{
    if(targetCell<0)return p;
    if(ClearLine(p,goal))return goal;
    int cell=Cell(p);if(cell<0)return p;
    int x=cell%width,y=cell/width,best=cell;
    // A collider-valid position can be inside a conservatively blocked grid cell.
    // Return a nearby reachable center; the actual movement still checks exact colliders.
    int radius=blocked[cell]?2:1;
    for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
        int nx=x+dx,ny=y+dy;if(!Walkable(nx,ny))continue;
        if(!blocked[cell]&&dx&&dy&&(!Walkable(nx,y)||!Walkable(x,ny)))continue;
        int next=ny*width+nx;if(distance[next]<distance[best])best=next;
    }
    return distance[best]<Unreachable?Center(best):p;
}
