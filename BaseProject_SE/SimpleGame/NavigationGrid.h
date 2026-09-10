#pragma once
#include "Renderer.h"

// Shared distance field: one search from the player's cell serves every enemy.
// Conservative obstacle rasterization keeps diagonal movement away from corners.
class NavigationGrid {
public:
    void Reset(int cells,float cellSize);
    void BlockCircle(Vec2 center,float radius);
    void BlockRectangle(Vec2 minimum,Vec2 maximum);
    void UpdateTarget(Vec2 target);
    Vec2 Waypoint(Vec2 position) const;
private:
    int width=0,targetCell=-1;
    float size=60;
    Vec2 goal;
    std::vector<unsigned char> blocked;
    std::vector<int> distance;
    bool Walkable(int x,int y) const;
    int Cell(Vec2 position) const;
    Vec2 Center(int index) const;
    bool ClearLine(Vec2 from,Vec2 to) const;
};
