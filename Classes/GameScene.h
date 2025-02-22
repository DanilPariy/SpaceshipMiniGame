#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include "cocos2d.h"
#include <unordered_set>

class GameScene
    : public cocos2d::Scene
{
private:
    struct sSpaceshipConfig
    {
        float linearDamping;
        float mass;
        float acceleration;
        float bulletCooldown;

        sSpaceshipConfig()
            : linearDamping(1.f)
            , mass(1.f)
            , acceleration(500.f)
            , bulletCooldown(0.3f)
        {
        }
    };

    struct sBulletConfig
    {
        float acceleration;

        sBulletConfig()
            : acceleration(500.f)
        {
        }
    };

    struct sAsteroidStageConfig
    {
        float scale;
        float mass;
        unsigned points;
    };

    struct sContactData
    {
        cocos2d::Vec2 position;
        cocos2d::Vec2 velocity;
        float mass;

        sContactData(cocos2d::PhysicsBody& aBody)
            : position(aBody.getPosition())
            , velocity(aBody.getVelocity())
            , mass(aBody.getMass())
        {
        }
        virtual ~sContactData()
        {
        }
    };

    struct sAsteroidContactData : public sContactData
    {
        int tag;

        sAsteroidContactData(cocos2d::PhysicsBody& aBody, const int aTag)
            : sContactData(aBody)
            , tag(aTag)
        {
        }
        virtual ~sAsteroidContactData()
        {
        }
    };

private:
    cocos2d::Sprite* mSpaceship;
    std::unordered_set<cocos2d::EventKeyboard::KeyCode> mPressedKeys;
    cocos2d::Vec2 mMousePosition;
    sSpaceshipConfig mSpaceshipConfig;
    sBulletConfig mBulletConfig;
    bool mIsMousePressed;
    bool mIsCanShoot;
    std::map<float, sAsteroidStageConfig> mAsteroidStages;
    std::unordered_map<cocos2d::Node*, std::function<void()>> mDestroyedAsteroidsCallbacks;

private:
    bool init() override;
    void parseConfig();
    void createBackground();
    void createSpaceship();
    void createScreenBounds();
    void update(float aDelta) override;

    void onKeyPressed(cocos2d::EventKeyboard::KeyCode aKeyCode, cocos2d::Event* aEvent);
    void onKeyReleased(cocos2d::EventKeyboard::KeyCode aKeyCode, cocos2d::Event* aEvent);
    void onMouseMove(cocos2d::EventMouse* aEvent);
    void onMouseDown(cocos2d::EventMouse* aEvent);
    void onMouseUp(cocos2d::EventMouse* aEvent);
    bool onContactBegin(cocos2d::PhysicsContact& aCntact);
    bool onContactSeparate(cocos2d::PhysicsContact& aContact);
    bool onContactPreSolve(cocos2d::PhysicsContact& aContact, cocos2d::PhysicsContactPreSolve& aSolve);

    void shootBullet(cocos2d::Vec2 aTarget);
    void adjustSpaceshipRotation();
    void spawnAsteroid(float);
    void splitAsteroid(sAsteroidContactData aAsteroidData, sContactData aOtherBody, const cocos2d::Vec2 aContactPoint);

public:
    GameScene();
    virtual ~GameScene();

    static cocos2d::Scene* createScene();
};

#endif // __GAME_SCENE_H__
