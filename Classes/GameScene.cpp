#include "GameScene.h"
#include "json/document.h"
#include "json/filereadstream.h"
#include "json/reader.h"

USING_NS_CC;

const int spaceshipBitMask = 0x01;
const int asteroidBitMask = 0x02;
const int bulletBitMask = 0x04;
const int boundsBitmask = 0x08;

GameScene::GameScene()
    : mSpaceship(nullptr)
    , mIsCanShoot(true)
    , mIsMousePressed(false)
{

}

GameScene::~GameScene()
{

}

Scene* GameScene::createScene()
{
    Scene* ret = new (std::nothrow) GameScene();
    if (ret && ret->initWithPhysics() && ret->init())
    {
        ret->autorelease();
        return ret;
    }
    else
    {
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
}

void GameScene::parseConfig()
{
    std::string fileContent = FileUtils::getInstance()->getStringFromFile("config.json");

    rapidjson::Document document;
    document.Parse(fileContent.c_str());

    if (document.HasParseError())
        return;

    auto parseFloat = [](const rapidjson::Value& aSource, const char* aKey, float& aDestination)
    {
        if (aSource.HasMember(aKey) && aSource[aKey].IsFloat())
        {
            aDestination = aSource[aKey].GetFloat();
        }
    };

    if (document.HasMember("spaceship") && document["spaceship"].IsObject())
    {
        const auto& spaceshipJson = document["spaceship"];
        parseFloat(spaceshipJson, "linearDamping", mSpaceshipConfig.linearDamping);
        parseFloat(spaceshipJson, "mass", mSpaceshipConfig.mass);
        parseFloat(spaceshipJson, "acceleration", mSpaceshipConfig.acceleration);
        parseFloat(spaceshipJson, "bulletCooldown", mSpaceshipConfig.bulletCooldown);
    }

    if (document.HasMember("bullet") && document["bullet"].IsObject())
    {
        const auto& bulletJson = document["bullet"];
        parseFloat(bulletJson, "acceleration", mBulletConfig.acceleration);
    }
}

bool GameScene::init()
{
    if (_physicsWorld)
    {
        _physicsWorld->setGravity(Vec2(0, 0));
        _physicsWorld->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_ALL);
    }

    parseConfig();
    createScreenBounds();
    createBackground();
    createSpaceship();

    auto keyboardListener = EventListenerKeyboard::create();
    keyboardListener->onKeyPressed = CC_CALLBACK_2(GameScene::onKeyPressed, this);
    keyboardListener->onKeyReleased = CC_CALLBACK_2(GameScene::onKeyReleased, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);

    auto listener = EventListenerMouse::create();
    listener->onMouseMove = CC_CALLBACK_1(GameScene::onMouseMove, this);
    listener->onMouseDown = CC_CALLBACK_1(GameScene::onMouseDown, this);
    listener->onMouseUp = CC_CALLBACK_1(GameScene::onMouseUp, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);

    auto contactListener = EventListenerPhysicsContact::create();
    contactListener->onContactBegin = CC_CALLBACK_1(GameScene::onContactBegin, this);
    contactListener->onContactSeparate = CC_CALLBACK_1(GameScene::onContactSeparate, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(contactListener, this);

    schedule(CC_SCHEDULE_SELECTOR(GameScene::spawnAsteroid), 1.5f);

    this->scheduleUpdate();

    return true;
}

void GameScene::createScreenBounds()
{
    auto visibleSize = Director::getInstance()->getVisibleSize();
    auto origin = Director::getInstance()->getVisibleOrigin();

    auto edgeNode = Node::create();
    if (edgeNode)
    {
        edgeNode->setPosition(visibleSize.width / 2 + origin.x, visibleSize.height / 2 + origin.y);
        auto edgeBody = PhysicsBody::createEdgeBox(
            visibleSize,
            PhysicsMaterial(1.0f, 0.0f, 0.0f),
            3
        );
        if (edgeBody)
        {
            edgeBody->setDynamic(false);
            edgeBody->setCategoryBitmask(boundsBitmask);
            edgeBody->setCollisionBitmask(spaceshipBitMask);
            edgeBody->setContactTestBitmask(bulletBitMask);
            edgeNode->setPhysicsBody(edgeBody);
        }
    }

    this->addChild(edgeNode);
}

void GameScene::createBackground()
{
    const auto visibleSize = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    auto background = Sprite::create("background.jpg");
    if (background)
    {
        background->setPosition(visibleSize.width / 2 + origin.x, visibleSize.height / 2 + origin.y);
        this->addChild(background, 0);
    }
}

void GameScene::createSpaceship()
{
    const auto visibleSize = Director::getInstance()->getVisibleSize();
    mSpaceship = Sprite::create("spaceship.png");
    if (mSpaceship)
    {
        mSpaceship->setPosition(visibleSize.width / 2, visibleSize.height / 2);
        this->addChild(mSpaceship, 1);

        auto spaceshipBody = PhysicsBody::createCircle(mSpaceship->getContentSize().width / 2);
        if (spaceshipBody)
        {
            spaceshipBody->setCategoryBitmask(spaceshipBitMask);
            spaceshipBody->setCollisionBitmask(asteroidBitMask | boundsBitmask);
            spaceshipBody->setContactTestBitmask(asteroidBitMask);
            spaceshipBody->setDynamic(true);
            spaceshipBody->setRotationEnable(false);
            spaceshipBody->setLinearDamping(mSpaceshipConfig.linearDamping);
            spaceshipBody->setMass(mSpaceshipConfig.mass);
            mSpaceship->setPhysicsBody(spaceshipBody);
        }
    }
}

void GameScene::spawnAsteroid(float dt)
{
}

void GameScene::update(float aDelta)
{
    if (mSpaceship)
    {
        auto body = mSpaceship->getPhysicsBody();
        Vec2 force;

        if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_W) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_UP_ARROW))
        {
            force += Vec2(0, mSpaceshipConfig.acceleration);
        }
        if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_S) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_DOWN_ARROW))
        {
            force += Vec2(0, -mSpaceshipConfig.acceleration);
        }
        if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_A) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_LEFT_ARROW))
        {
            force += Vec2(-mSpaceshipConfig.acceleration, 0);
        }
        if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_D) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_RIGHT_ARROW))
        {
            force += Vec2(mSpaceshipConfig.acceleration, 0);
        }

        body->applyForce(force);
    }
    adjustSpaceshipRotation();
}

void GameScene::onKeyPressed(EventKeyboard::KeyCode aKeyCode, Event* aEvent)
{
    mPressedKeys.insert(aKeyCode);
}

void GameScene::onKeyReleased(EventKeyboard::KeyCode aKeyCode, Event* aEvent)
{
    mPressedKeys.erase(aKeyCode);
}

void GameScene::onMouseMove(EventMouse* aEvent)
{
    if (aEvent && mSpaceship)
    {
        mMousePosition = aEvent->getLocationInView();
        adjustSpaceshipRotation();
    }
}

void GameScene::adjustSpaceshipRotation()
{
    if (mSpaceship)
    {
        Vec2 direction = mMousePosition - mSpaceship->getPosition();
        float angle = CC_RADIANS_TO_DEGREES(atan2(direction.y, direction.x));
        // due to spaceship head looking up, we have to subtract 90 degrees
        angle -= 90;
        mSpaceship->setRotation(-angle);
    }
}

void GameScene::onMouseDown(EventMouse* aEvent)
{
    if (aEvent && aEvent->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT)
    {
        mIsMousePressed = true;
        if (mIsCanShoot)
        {
            shootBullet(aEvent->getLocationInView());
        }
    }
}

void GameScene::onMouseUp(cocos2d::EventMouse* aEvent)
{
    mIsMousePressed = false;
}

bool GameScene::onContactBegin(cocos2d::PhysicsContact& contact)
{
    return false;
}

bool GameScene::onContactSeparate(cocos2d::PhysicsContact& contact)
{
    auto shapeA = contact.getShapeA();
    auto shapeB = contact.getShapeB();

    auto bodyA = shapeA->getBody();
    auto bodyB = shapeB->getBody();

    if (!bodyA || !bodyB)
        return false;

    if ((bodyA->getCategoryBitmask() == bulletBitMask && bodyB->getCategoryBitmask() == boundsBitmask)
        || (bodyA->getCategoryBitmask() == boundsBitmask && bodyB->getCategoryBitmask() == bulletBitMask))
    {
        if (bodyA->getCategoryBitmask() == bulletBitMask)
        {
            if (bodyA->getNode())
                bodyA->getNode()->removeFromParent();
        }
        if (bodyB->getCategoryBitmask() == bulletBitMask)
        {
            if (bodyB->getNode())
                bodyB->getNode()->removeFromParent();
        }
    }

    return true;
}

void GameScene::shootBullet(Vec2 aTarget)
{
    if (mSpaceship)
    {
        auto bullet = Sprite::create("missile.png");
        if (bullet)
        {
            mIsCanShoot = false;

            bullet->setPosition(mSpaceship->getPosition());
            this->addChild(bullet);

            Vec2 direction = aTarget - bullet->getPosition();
            direction.normalize();
            Vec2 velocity = direction * mBulletConfig.acceleration;

            auto bulletBody = PhysicsBody::createBox(bullet->getContentSize());
            if (bulletBody)
            {
                bulletBody->setCategoryBitmask(bulletBitMask);
                bulletBody->setContactTestBitmask(boundsBitmask);
                bulletBody->setVelocity(velocity);
                bulletBody->setGravityEnable(false);
                bulletBody->setRotationEnable(false);
                bullet->setPhysicsBody(bulletBody);
            }
            bullet->setRotation(mSpaceship->getRotation());

            this->scheduleOnce([this](float)
                {
                    mIsCanShoot = true;
                    if (mIsMousePressed)
                        shootBullet(mMousePosition);
                }, mSpaceshipConfig.bulletCooldown, "fireCooldown"
            );
        }
    }
}
