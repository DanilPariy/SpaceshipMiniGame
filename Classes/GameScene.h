#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include "cocos2d.h"
#include <unordered_set>

class GameScene
    : public cocos2d::Scene
{
private:
    cocos2d::Sprite* mSpaceship;
    std::unordered_set<cocos2d::EventKeyboard::KeyCode> mPressedKeys;
    cocos2d::Vec2 mMousePosition;
    float mBulletCooldown;
    bool mIsCanShoot;

private:
    bool init() override;
    void update(float aDelta) override;

    void onKeyPressed(cocos2d::EventKeyboard::KeyCode aKeyCode, cocos2d::Event* aEvent);
    void onKeyReleased(cocos2d::EventKeyboard::KeyCode aKeyCode, cocos2d::Event* aEvent);
    void onMouseMove(cocos2d::EventMouse* aEvent);
    void onMouseDown(cocos2d::EventMouse* aEvent);

    void shootBullet(cocos2d::Vec2 target);
    void adjustSpaceshipRotation();

public:
    GameScene();
    virtual ~GameScene();

    static cocos2d::Scene* createScene();

    CREATE_FUNC(GameScene);
};

#endif // __GAME_SCENE_H__
