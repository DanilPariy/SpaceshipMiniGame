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
    , mIsPaused(false)
    , mPauseLabel(nullptr)
    , mBlackoutLayer(nullptr)
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

    if (document.HasMember("asteroid") && document["asteroid"].IsArray())
    {
        const auto& asteroidConfig = document["asteroid"];
        for (auto it = asteroidConfig.Begin(); it != asteroidConfig.End(); it++)
        {
            const auto& stageObj = *it;
            if (stageObj.IsObject() && stageObj.HasMember("scale") && stageObj.HasMember("mass") && stageObj.HasMember("points"))
            {
                sAsteroidStageConfig config(
                    {
                        .scale = stageObj["scale"].GetFloat(),
                        .mass = stageObj["mass"].GetFloat(),
                        .points = stageObj["points"].GetUint()
                    }
                );
                mAsteroidStages.insert(std::make_pair(config.scale, std::move(config)));
            }
        }
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
    contactListener->onContactPreSolve = CC_CALLBACK_2(GameScene::onContactPreSolve, this);
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

void GameScene::spawnAsteroid(float)
{
    const auto visibleSize = Director::getInstance()->getVisibleSize();
    auto asteroidSprite = Sprite::create("asteroid.png");
    if (asteroidSprite)
    {
        auto index = RandomHelper::random_int(0u, mAsteroidStages.size() - 1);
        auto stage = std::next(mAsteroidStages.begin(), index);
        auto scale = stage->second.scale;
        asteroidSprite->setScale(scale);
        asteroidSprite->setTag(index);
        // Випадкова стартова позиція за межами екрану
        Vec2 spawnPosition = Vec2(visibleSize.width * RandomHelper::random_int(0, 1), visibleSize.height * RandomHelper::random_int(0, 1));
        asteroidSprite->setPosition(spawnPosition);
        this->addChild(asteroidSprite);

        auto asteroidBody = PhysicsBody::createCircle(asteroidSprite->getContentSize().width / 2);
        if (asteroidBody)
        {
            asteroidSprite->setPhysicsBody(asteroidBody);
            asteroidBody->setDynamic(true);
            asteroidBody->setGravityEnable(false);
            asteroidBody->setMass(stage->second.mass);
            asteroidBody->setCategoryBitmask(asteroidBitMask);
            asteroidBody->setCollisionBitmask(spaceshipBitMask | bulletBitMask | asteroidBitMask);
            asteroidBody->setContactTestBitmask(spaceshipBitMask | bulletBitMask | asteroidBitMask);

            // Напрямок руху до центра екрану
            Vec2 direction = (Vec2(visibleSize.width / 2, visibleSize.height / 2) - spawnPosition).getNormalized();
            float speed = RandomHelper::random_real(100.0f, 250.0f);
            asteroidBody->setVelocity(direction * speed);
        }
    }
}

void GameScene::update(float)
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

    for (auto it = mDestroyedAsteroidsCallbacks.begin(); it != mDestroyedAsteroidsCallbacks.end(); )
    {
        auto asteroid = it->first;
        auto callback = it->second;
        if (!callback)
        {
            it = mDestroyedAsteroidsCallbacks.erase(it);
            continue;
        }

        auto sceneChildren = getChildren();
        auto findIt = sceneChildren.find(asteroid);
        if (findIt == sceneChildren.end())
        {
            callback();
            it = mDestroyedAsteroidsCallbacks.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void GameScene::onKeyPressed(EventKeyboard::KeyCode aKeyCode, Event* aEvent)
{
    if (aKeyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
    {
        togglePause();
    }
    else if (!mIsPaused)
    {
        mPressedKeys.insert(aKeyCode);
    }
}

void GameScene::onKeyReleased(EventKeyboard::KeyCode aKeyCode, Event* aEvent)
{
    if (!mIsPaused)
    {
        mPressedKeys.erase(aKeyCode);
    }
}

void GameScene::onMouseMove(EventMouse* aEvent)
{
    if (aEvent && mSpaceship)
    {
        mMousePosition = aEvent->getLocationInView();
        if (!mIsPaused)
        {
            adjustSpaceshipRotation();
        }
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

void GameScene::onMouseUp(EventMouse* aEvent)
{
    mIsMousePressed = false;
}

void GameScene::togglePause()
{
    auto director = Director::getInstance();
    if (mIsPaused)
    {
        if (mPauseLabel)
        {
            mPauseLabel->setVisible(false);
        }
        if (mBlackoutLayer)
        {
            mBlackoutLayer->setVisible(false);
        }
        director->resume();
        if (_physicsWorld)
        {
            _physicsWorld->setSpeed(1.0f);
        }
        this->scheduleUpdate();
    }
    else
    {
        if (!mPauseLabel)
        {
            mPauseLabel = Label::createWithTTF("PAUSED", "fonts/Marker Felt.ttf", 48);
            mPauseLabel->setPosition(Director::getInstance()->getVisibleSize() / 2);
            this->addChild(mPauseLabel, 11);
        }
        if (mPauseLabel)
        {
            mPauseLabel->setVisible(true);
        }
        if (!mBlackoutLayer)
        {
            mBlackoutLayer = LayerColor::create(Color4B(0, 0, 0, 150));
            mBlackoutLayer->setContentSize(Director::getInstance()->getVisibleSize());
            mBlackoutLayer->setPosition(Vec2::ZERO);
            this->addChild(mBlackoutLayer, 10);
        }
        if (mBlackoutLayer)
        {
            mBlackoutLayer->setVisible(true);
        }
        director->pause();
        if (_physicsWorld)
        {
            _physicsWorld->setSpeed(0.0f);
        }
        this->unscheduleUpdate();
    }
    mIsPaused = !mIsPaused;
}

Vec2 GameScene::calculateVelocityAfterCollision(Vec2 v1, Vec2 v2, float m1, float m2, Vec2 p1, Vec2 p2)
{
    Vec2 dp = p1 - p2;
    float dpLengthSq = dp.lengthSquared();
    if (dpLengthSq == 0)
        return v1;

    Vec2 dv = v1 - v2;
    float dot = dv.dot(dp);
    Vec2 newVelocity = v1 - (2 * m2 / (m1 + m2)) * (dot / dpLengthSq) * dp;
    return newVelocity;
}

void GameScene::splitAsteroid(sAsteroidContactData aAsteroidData, sContactData aOtherBody, const Vec2 aContactPoint)
{
    auto index = aAsteroidData.tag;
    if (index == 0)
        return;

    index--;
    const auto stage = std::next(mAsteroidStages.begin(), index);
    const auto scale = stage->second.scale;
    const auto mass = stage->second.mass;

    const Vec2 asteroidPosition = aAsteroidData.position;
    const Vec2 asteroidVelocityAfterCollision = calculateVelocityAfterCollision(
        aAsteroidData.velocity, aOtherBody.velocity, aAsteroidData.mass, aOtherBody.mass, asteroidPosition, aOtherBody.position);

    float angle45 = CC_DEGREES_TO_RADIANS(45.0f);
    auto velocity1 = asteroidVelocityAfterCollision.rotate(Vec2::forAngle(angle45)) / 2.f;
    auto velocity2 = asteroidVelocityAfterCollision.rotate(Vec2::forAngle(-angle45)) / 2.f;

    auto createSmallAsteroid = [](const float aScale, const int aIndex, const float aMass, const Vec2 aVelocity) -> Sprite*
    {
        auto smallAsteroid = Sprite::create("asteroid.png");
        if (smallAsteroid)
        {
            smallAsteroid->setScale(aScale);
            smallAsteroid->setTag(aIndex);
            auto body = PhysicsBody::createCircle(smallAsteroid->getContentSize().width / 2);
            if (body)
            {
                body->setDynamic(true);
                body->setGravityEnable(false);
                body->setMass(aMass);
                body->setCategoryBitmask(asteroidBitMask);
                body->setCollisionBitmask(spaceshipBitMask | bulletBitMask | asteroidBitMask);
                body->setContactTestBitmask(spaceshipBitMask | bulletBitMask | asteroidBitMask);
                body->setVelocity(aVelocity);
                smallAsteroid->setPhysicsBody(body);
            }
        }
        return smallAsteroid;
    };

    auto first = createSmallAsteroid(scale, index, mass, velocity1);
    auto second = createSmallAsteroid(scale, index, mass, velocity2);
    if (first && second)
    {
        float offset = first->getContentSize().width / (2 * std::sin(angle45));
        auto position1 = asteroidPosition + velocity1.getNormalized() * offset;
        auto position2 = asteroidPosition + velocity2.getNormalized() * offset;
        first->setPosition(position1);
        second->setPosition(position2);
        addChild(first);
        addChild(second);
    }
}

bool GameScene::onContactPreSolve(PhysicsContact& aContact, PhysicsContactPreSolve& aSolve)
{
    if (!aContact.getShapeA() || !aContact.getShapeB())
        return false;

    auto bodyA = aContact.getShapeA()->getBody();
    auto bodyB = aContact.getShapeB()->getBody();

    return true;
}

bool GameScene::onContactBegin(PhysicsContact& aContact)
{
    auto shapeA = aContact.getShapeA();
    auto shapeB = aContact.getShapeB();
    if (!shapeA || !shapeB)
        return false;

    auto bodyA = shapeA->getBody();
    auto bodyB = shapeB->getBody();
    if (!bodyA || !bodyB)
        return false;

    auto contactPoint = aContact.getContactData()->points[0];

    if (bodyA->getCategoryBitmask() == asteroidBitMask && bodyB->getCategoryBitmask() == spaceshipBitMask)
    {
        // gameover
    }
    else if (bodyA->getCategoryBitmask() == spaceshipBitMask && bodyB->getCategoryBitmask() == asteroidBitMask)
    {
        // gameover
    }
    else if (bodyA->getCategoryBitmask() == asteroidBitMask && bodyB->getCategoryBitmask() == asteroidBitMask)
    {
        //splitAsteroid(bodyA, bodyB, contactPoint);
        //splitAsteroid(bodyB, bodyA, contactPoint);
        //if (bodyA->getNode())
        //    bodyA->getNode()->removeFromParent();
        //if (bodyB->getNode())
        //    bodyB->getNode()->removeFromParent();
    }
    else if ((bodyA->getCategoryBitmask() == asteroidBitMask && bodyB->getCategoryBitmask() == bulletBitMask)
        || (bodyA->getCategoryBitmask() == bulletBitMask && bodyB->getCategoryBitmask() == asteroidBitMask))
    {
        auto asteroid = bodyA->getCategoryBitmask() == asteroidBitMask ? bodyA : bodyB;
        auto bullet = bodyA->getCategoryBitmask() == bulletBitMask ? bodyA : bodyB;
        if (asteroid->getNode() && bullet->getNode())
        {
            sAsteroidContactData asteroidData(*asteroid, asteroid->getNode()->getTag());
            sContactData bulletData(*bullet);
            mDestroyedAsteroidsCallbacks[bodyA->getNode()] = CC_CALLBACK_0(GameScene::splitAsteroid, this, asteroidData, bulletData, contactPoint);
            asteroid->getNode()->removeFromParent();
            bullet->getNode()->removeFromParent();
        }
        else if (asteroid->getNode())
        {
            asteroid->getNode()->removeFromParent();
        }
        else if (bullet->getNode())
        {
            bullet->getNode()->removeFromParent();
        }
    }

    return true;
}

bool GameScene::onContactSeparate(PhysicsContact& aContact)
{
    auto shapeA = aContact.getShapeA();
    auto shapeB = aContact.getShapeB();

    auto bodyA = shapeA->getBody();
    auto bodyB = shapeB->getBody();

    if (!bodyA || !bodyB)
        return false;

    if (bodyA->getCategoryBitmask() == bulletBitMask && bodyB->getCategoryBitmask() == boundsBitmask)
    {
        if (bodyA->getNode())
            bodyA->getNode()->removeFromParent();
    }
    else if (bodyA->getCategoryBitmask() == boundsBitmask && bodyB->getCategoryBitmask() == bulletBitMask)
    {
        if (bodyB->getNode())
            bodyB->getNode()->removeFromParent();
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
                bulletBody->setCollisionBitmask(asteroidBitMask);
                bulletBody->setContactTestBitmask(boundsBitmask | asteroidBitMask);
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
