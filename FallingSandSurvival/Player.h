
#define INC_Player

#ifndef INC_Entity
#include "Entity.h"
#endif
#include "Item.h"

#ifndef INC_World
#include "world.h"
#endif

class World;

class Player : public Entity {
public:
    Item* heldItem = nullptr;
    float holdAngle = 0;
    long long startThrow = 0;
    bool holdHammer = false;
    bool holdVacuum = false;
    int hammerX = 0;
    int hammerY = 0;

    void render(GPU_Target* target, int ofsX, int ofsY) override;
    void setItemInHand(Item* item, World* world);

    ~Player();
};
