#include "GameScene.h"

#include "json/document.h"
#include "json/filereadstream.h"
#include "json/reader.h"
#include "GameOverLayer.h"

const int spaceshipBitMask = 0x01;
const int asteroidBitMask = 0x02;
const int bulletBitMask = 0x04;
const int boundsBitmask = 0x08;

const char* bulletExpl = "expl_11";
const int bulletExplFrames = 24;
const char* asteroidExpl = "expl_11";
const int asteroidExplFrames = 24;

void loadFramesFromJSON(const std::string& aJsonFile, const std::string& aTextureFile)
{
    auto texture = Director::getInstance()->getTextureCache()->addImage(aTextureFile);
    if (!texture)
        return;

    std::string jsonContent = FileUtils::getInstance()->getStringFromFile(aJsonFile);

    rapidjson::Document document;
    document.Parse(jsonContent.c_str());

    if (document.HasParseError() || !document.HasMember("frames"))
        return;

    const rapidjson::Value& frames = document["frames"];
    for (auto it = frames.MemberBegin(); it != frames.MemberEnd(); ++it)
    {
        std::string frameName = it->name.GetString();
        const rapidjson::Value& frameData = it->value["frame"];

        float x = frameData["x"].GetFloat();
        float y = frameData["y"].GetFloat();
        float w = frameData["w"].GetFloat();
        float h = frameData["h"].GetFloat();

        auto spriteFrame = SpriteFrame::createWithTexture(texture, Rect(x, y, w, h));
        if (spriteFrame)
        {
            SpriteFrameCache::getInstance()->addSpriteFrame(spriteFrame, frameName);
        }
    }
}

void loadAnimation(const std::string& aName, unsigned aFramesNumber)
{
    Vector<SpriteFrame*> frames;
    for (int i = 0; i < aFramesNumber; i++)
    {
        std::string frameName = StringUtils::format("%s_%04d.png", aName.c_str(), i);
        auto frame = SpriteFrameCache::getInstance()->getSpriteFrameByName(frameName);
        if (frame)
        {
            frames.pushBack(frame);
        }
    }

    if (!frames.empty())
    {
        auto animation = Animation::createWithSpriteFrames(frames, 1.0f / 60);
        AnimationCache::getInstance()->addAnimation(animation, aName);
    }
}

GameScene::GameScene()
    : mSpaceship(nullptr)
    , mIsCanShoot(true)
    , mIsMousePressed(false)
    , mIsPaused(false)
    , mPauseLabel(nullptr)
    , mBlackoutLayer(nullptr)
    , mContactListener(nullptr)
    , mMouseListener(nullptr)
    , mKeyboardListener(nullptr)
    , mCurrentStage(nullptr)
    , mScore(0)
    , mScoreLabel(nullptr)
    , mTimeLeftLabel(nullptr)
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
        parseFloat(bulletJson, "velocity", mBulletConfig.velocity);
        parseFloat(bulletJson, "mass", mBulletConfig.mass);
    }

    if (document.HasMember("asteroid") && document["asteroid"].IsArray())
    {
        const auto& asteroidConfig = document["asteroid"];
        for (auto it = asteroidConfig.Begin(); it != asteroidConfig.End(); it++)
        {
            const auto& stageObj = *it;
            if (stageObj.IsObject()
                && stageObj.HasMember("scale")
                && stageObj.HasMember("mass")
                && stageObj.HasMember("points")
                && stageObj.HasMember("velocity_min")
                && stageObj.HasMember("velocity_max"))
            {
                sAsteroidStageConfig config(
                    {
                        .scale = stageObj["scale"].GetFloat()
                        , .mass = stageObj["mass"].GetFloat()
                        , .points = stageObj["points"].GetUint()
                        , .velocityMin = stageObj["velocity_min"].GetFloat()
                        , .velocityMax = stageObj["velocity_max"].GetFloat()
                    }
                );
                mAsteroidStages.insert(std::make_pair(config.scale, std::move(config)));
            }
        }
    }

    if (document.HasMember("stages") && document["stages"].IsArray())
    {
        const auto& stagesConfig = document["stages"];
        for (auto it = stagesConfig.Begin(); it != stagesConfig.End(); it++)
        {
            const auto& stageObj = *it;
            if (stageObj.IsObject()
                && stageObj.HasMember("center_min_x")
                && stageObj.HasMember("center_max_x")
                && stageObj.HasMember("center_min_y")
                && stageObj.HasMember("center_max_y")
                && stageObj.HasMember("radius_min")
                && stageObj.HasMember("radius_max")
                && stageObj.HasMember("time_min")
                && stageObj.HasMember("time_max"))
            {
                sStageConfig config(
                    {
                        .centerMinX = stageObj["center_min_x"].GetFloat()
                        , .centerMaxX = stageObj["center_max_x"].GetFloat()
                        , .centerMinY = stageObj["center_min_y"].GetFloat()
                        , .centerMaxY = stageObj["center_max_y"].GetFloat()
                        , .radiusMin = stageObj["radius_min"].GetFloat()
                        , .radiusMax = stageObj["radius_max"].GetFloat()
                        , .timeMin = stageObj["time_min"].GetUint()
                        , .timeMax = stageObj["time_max"].GetUint()
                    }
                );
                mStageConfigs.push_back(config);
            }
        }
    }
}

bool GameScene::init()
{
    if (_physicsWorld)
    {
        _physicsWorld->setGravity(Vec2(0, 0));
        _physicsWorld->setDebugDrawMask(0);// PhysicsWorld::DEBUGDRAW_ALL);
    }

    mGameStartTime = std::chrono::steady_clock::now();

    parseConfig();
    createScreenBounds();
    createBackground();
    createSpaceship();
    createListeners();
    switchStage();
    updateScoreLabel();
    updateTimeLeftLabel();

    loadFramesFromJSON("explosions.json", "explosions.png");
    loadAnimation(bulletExpl, bulletExplFrames);
    loadAnimation(asteroidExpl, asteroidExplFrames);

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
        auto edgeBody = PhysicsBody::createEdgeBox(visibleSize);
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

void GameScene::createListeners()
{
    mKeyboardListener = EventListenerKeyboard::create();
    if (mKeyboardListener)
    {
        mKeyboardListener->onKeyPressed = CC_CALLBACK_2(GameScene::onKeyPressed, this);
        mKeyboardListener->onKeyReleased = CC_CALLBACK_2(GameScene::onKeyReleased, this);
        _eventDispatcher->addEventListenerWithSceneGraphPriority(mKeyboardListener, this);
    }

    mMouseListener = EventListenerMouse::create();
    if (mMouseListener)
    {
        mMouseListener->onMouseMove = CC_CALLBACK_1(GameScene::onMouseMove, this);
        mMouseListener->onMouseDown = CC_CALLBACK_1(GameScene::onMouseDown, this);
        mMouseListener->onMouseUp = CC_CALLBACK_1(GameScene::onMouseUp, this);
        _eventDispatcher->addEventListenerWithSceneGraphPriority(mMouseListener, this);
    }

    mContactListener = EventListenerPhysicsContact::create();
    if (mContactListener)
    {
        mContactListener->onContactBegin = CC_CALLBACK_1(GameScene::onContactBegin, this);
        mContactListener->onContactSeparate = CC_CALLBACK_1(GameScene::onContactSeparate, this);
        _eventDispatcher->addEventListenerWithSceneGraphPriority(mContactListener, this);
    }
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
        const auto index = RandomHelper::random_int(0u, mAsteroidStages.size() - 1);
        auto stage = std::next(mAsteroidStages.begin(), index);
        const auto scale = stage->second.scale;
        asteroidSprite->setScale(scale);
        asteroidSprite->setTag(index);
        const Vec2 spawnPosition = Vec2(visibleSize.width * RandomHelper::random_int(0, 1), visibleSize.height * RandomHelper::random_int(0, 1));
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
            asteroidBody->setContactTestBitmask(spaceshipBitMask | bulletBitMask);

            Vec2 destination;
            if (mCurrentStage)
            {
                const float radiusPart = RandomHelper::random_real(0.f, mCurrentStage->radius);
                const auto directionFromCenter = Vec2::forAngle(RandomHelper::random_real(0.f, 1.f) * 2 * M_PI);
                destination = Vec2(mCurrentStage->centerX, mCurrentStage->centerY) + directionFromCenter * radiusPart;
            }
            else
            {
                destination = Vec2(RandomHelper::random_real(0.f, visibleSize.width), RandomHelper::random_real(0.f, visibleSize.height));
            }
            const Vec2 direction = (destination - spawnPosition).getNormalized();
            const float speed = RandomHelper::random_real(stage->second.velocityMin, stage->second.velocityMax);
            asteroidBody->setVelocity(direction * speed);
        }
    }
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

    if (mCurrentStage && mSpaceship)
    {
        Vec2 shipPosition = mSpaceship->getPosition();
        float distance = shipPosition.distance(Vec2(mCurrentStage->centerX, mCurrentStage->centerY));
        if (distance <= mCurrentStage->radius - mSpaceship->getContentSize().width / 2)
        {
            mCurrentStage->timeInZone += aDelta;
            if (mCurrentStage->timeInZone >= mCurrentStage->timeNeeded)
            {
                switchStage();
            }
        }
        else
        {
            mCurrentStage->timeInZone = 0.f;
        }
        updateTimeLeftLabel();
    }
}

void GameScene::updateScoreLabel()
{
    if (!mScoreLabel)
    {
        mScoreLabel = Label::createWithTTF("Score: 0", "fonts/Marker Felt.ttf", 24);
        if (mScoreLabel)
        {
            mScoreLabel->setAnchorPoint(Vec2(0, 1));
            mScoreLabel->setPosition(Vec2(10, Director::getInstance()->getVisibleSize().height - 10));
            this->addChild(mScoreLabel);
        }
    }
    else
    {
        mScoreLabel->setString(StringUtils::format("Score: %d", mScore));
    }
}

void GameScene::updateTimeLeftLabel()
{
    if (mCurrentStage)
    {
        if (!mTimeLeftLabel && mScoreLabel)
        {
            mTimeLeftLabel = Label::createWithTTF("Time left: 0.0", "fonts/Marker Felt.ttf", 24);
            if (mTimeLeftLabel)
            {
                mTimeLeftLabel->setAnchorPoint(Vec2(0, 1)); // Прив'язка до лівого верхнього кута
                mTimeLeftLabel->setPosition(mScoreLabel->getPosition() - Vec2(0, mScoreLabel->getContentSize().height + 5));
                this->addChild(mTimeLeftLabel);
            }
        }
        else
        {
            auto timeLeft = std::max(mCurrentStage->timeNeeded - mCurrentStage->timeInZone, 0.f);
            mTimeLeftLabel->setString(StringUtils::format("Time left: %.1f", timeLeft));
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

void GameScene::splitAsteroid(sAsteroidContactData aAsteroidData, sContactData aOtherBody)
{
    auto index = aAsteroidData.tag;
    mScore += std::next(mAsteroidStages.begin(), index)->second.points;
    updateScoreLabel();
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
                body->setContactTestBitmask(spaceshipBitMask | bulletBitMask);
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

    if ((bodyA->getCategoryBitmask() == asteroidBitMask && bodyB->getCategoryBitmask() == spaceshipBitMask)
        || (bodyA->getCategoryBitmask() == spaceshipBitMask && bodyB->getCategoryBitmask() == asteroidBitMask))
    {
        auto spaceship = bodyA->getCategoryBitmask() == spaceshipBitMask ? bodyA : bodyB;
        if (spaceship)
        {
            createExplosion(asteroidExpl, spaceship->getPosition());
            if (spaceship->getNode())
            {
                spaceship->getNode()->removeFromParent();
                mSpaceship = nullptr;
            }
        }
        gameOver(false);
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
            mDestroyedAsteroidsCallbacks[bodyA->getNode()] = CC_CALLBACK_0(GameScene::splitAsteroid, this, asteroidData, bulletData);
            createExplosion(bulletExpl, asteroid->getPosition());
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
            Vec2 velocity = direction * mBulletConfig.velocity;

            auto bulletBody = PhysicsBody::createBox(bullet->getContentSize());
            if (bulletBody)
            {
                bulletBody->setMass(mBulletConfig.mass);
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

void GameScene::gameOver(bool aIsWin)
{
    auto gameOverTime = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(gameOverTime - mGameStartTime).count();
    auto gameOverLayer = GameOverLayer::create(mScore, elapsedTime / 1000.f, aIsWin);
    this->addChild(gameOverLayer, 10);
    if (_physicsWorld)
    {
        _physicsWorld->setSpeed(0.0f);
    }
    if (mContactListener)
    {
        _eventDispatcher->removeEventListener(mContactListener);
        mContactListener = nullptr;
    }
    if (mMouseListener)
    {
        _eventDispatcher->removeEventListener(mMouseListener);
        mMouseListener = nullptr;
    }
    if (mKeyboardListener)
    {
        _eventDispatcher->removeEventListener(mKeyboardListener);
        mKeyboardListener = nullptr;
    }
    unschedule(CC_SCHEDULE_SELECTOR(GameScene::spawnAsteroid));
    this->unscheduleUpdate();
}

void GameScene::switchStage()
{
    if (mStageConfigs.empty())
        return;

    auto nextStageConfigIt = mCurrentStage ? std::next(std::find_if(mStageConfigs.begin(), mStageConfigs.end(), [this](const auto& aStage) {return mCurrentStage->config == &aStage; })) : mStageConfigs.begin();
    if (nextStageConfigIt == mStageConfigs.end())
    {
        gameOver(true);
        return;
    }

    mCurrentStage = std::make_unique<sStage>(sStage
        {
            .config = &*nextStageConfigIt
            , .centerX = RandomHelper::random_real(nextStageConfigIt->centerMinX, nextStageConfigIt->centerMaxX)
            , .centerY = RandomHelper::random_real(nextStageConfigIt->centerMinY, nextStageConfigIt->centerMaxY)
            , .radius = RandomHelper::random_real(nextStageConfigIt->radiusMin, nextStageConfigIt->radiusMax)
            , .timeNeeded = RandomHelper::random_int(nextStageConfigIt->timeMin, nextStageConfigIt->timeMax)
            , .timeInZone = 0.f
        }
    );

    if (mCurrentStage)
    {
        auto drawNode = getChildByName<DrawNode*>("stage_circle");
        if (drawNode)
        {
            drawNode->clear();
        }
        else
        {
            drawNode = DrawNode::create();
            if (drawNode)
            {
                drawNode->setName("stage_circle");
                this->addChild(drawNode);
            }
        }
        if (drawNode)
        {
            drawNode->drawCircle(Vec2(mCurrentStage->centerX, mCurrentStage->centerY), mCurrentStage->radius, 0, 100, false, Color4F::GREEN);
        }
    }
}

void GameScene::createExplosion(const std::string& aAnimationName, const Vec2& aPosition)
{
    auto animation = AnimationCache::getInstance()->getAnimation(aAnimationName);
    if (animation)
    {
        auto explosion = Sprite::createWithSpriteFrame(animation->getFrames().front()->getSpriteFrame());
        explosion->setPosition(aPosition);
        this->addChild(explosion, 2);

        auto animate = Animate::create(animation);
        auto remove = RemoveSelf::create();
        explosion->runAction(Sequence::createWithTwoActions(animate, remove));
    }
}
