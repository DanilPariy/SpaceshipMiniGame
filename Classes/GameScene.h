#ifndef __GAME_SCENE_H__
#define __GAME_SCENE_H__

#include "cocos2d.h"
#include <unordered_set>

USING_NS_CC;

class GameScene
    : public Scene
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
        float mass;
        float acceleration;

        sBulletConfig()
            : mass(1.f)
            , acceleration(500.f)
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
        Vec2 position;
        Vec2 velocity;
        float mass;

        sContactData(PhysicsBody& aBody)
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

        sAsteroidContactData(PhysicsBody& aBody, const int aTag)
            : sContactData(aBody)
            , tag(aTag)
        {
        }
        virtual ~sAsteroidContactData()
        {
        }
    };

private:
    Sprite* mSpaceship;
    std::unordered_set<EventKeyboard::KeyCode> mPressedKeys;
    Vec2 mMousePosition;
    sSpaceshipConfig mSpaceshipConfig;
    sBulletConfig mBulletConfig;
    bool mIsMousePressed;
    bool mIsCanShoot;
    std::map<float, sAsteroidStageConfig> mAsteroidStages;
    std::unordered_map<Node*, std::function<void()>> mDestroyedAsteroidsCallbacks;

    bool mIsPaused;
    Label* mPauseLabel;
    LayerColor* mBlackoutLayer;

    EventListenerKeyboard* mKeyboardListener;
    EventListenerMouse* mMouseListener;
    EventListenerPhysicsContact* mContactListener;

private:
    bool init() override;
    void parseConfig();
    void createBackground();
    void createSpaceship();
    void createScreenBounds();
    void update(float aDelta) override;

    void onKeyPressed(EventKeyboard::KeyCode aKeyCode, Event* aEvent);
    void onKeyReleased(EventKeyboard::KeyCode aKeyCode, Event* aEvent);
    void onMouseMove(EventMouse* aEvent);
    void onMouseDown(EventMouse* aEvent);
    void onMouseUp(EventMouse* aEvent);
    bool onContactBegin(PhysicsContact& aCntact);
    bool onContactSeparate(PhysicsContact& aContact);
    bool onContactPreSolve(PhysicsContact& aContact, PhysicsContactPreSolve& aSolve);

    void shootBullet(Vec2 aTarget);
    void adjustSpaceshipRotation();
    void spawnAsteroid(float);
    void splitAsteroid(sAsteroidContactData aAsteroidData, sContactData aOtherBody);
    static Vec2 calculateVelocityAfterCollision(Vec2 v1, Vec2 v2, float m1, float m2, Vec2 p1, Vec2 p2);

    void togglePause();
    void gameOver(int aScore, float aTime, bool aIsWin);

public:
    GameScene();
    virtual ~GameScene();

    static Scene* createScene();
};

#endif // __GAME_SCENE_H__
