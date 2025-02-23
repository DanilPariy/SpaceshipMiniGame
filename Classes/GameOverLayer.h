#ifndef __GAME_OVER_LAYER_H__
#define __GAME_OVER_LAYER_H__

#include "cocos2d.h"

USING_NS_CC;

class GameOverLayer
    : public Layer
{
public:
    GameOverLayer();
    virtual ~GameOverLayer();

    static GameOverLayer* create(int aScore, float aTime, bool aIsWin);

    bool init(int aScore, float aTime, bool aIsWin);
};

#endif // __GAME_OVER_LAYER_H__
