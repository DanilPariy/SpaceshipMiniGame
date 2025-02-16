#include "GameScene.h"
#include "SimpleAudioEngine.h"

USING_NS_CC;

GameScene::GameScene()
    : mSpaceship(nullptr)
    , mBulletCooldown(0.3f)
    , mIsCanShoot(true)
{

}

GameScene::~GameScene()
{

}

Scene* GameScene::createScene()
{
    auto scene = GameScene::create();
    if (!scene)
        return nullptr;
    return scene;
}

bool GameScene::init()
{
    if (!Scene::init())
        return false;

    const auto visibleSize = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    auto background = Sprite::create("background.jpg");
    if (background)
    {
        background->setPosition(visibleSize.width / 2 + origin.x, visibleSize.height / 2 + origin.y);
        this->addChild(background, 0);
    }

    mSpaceship = Sprite::create("spaceship.png");
    if (mSpaceship)
    {
        mSpaceship->setPosition(visibleSize.width / 2, visibleSize.height / 2);
        this->addChild(mSpaceship, 1);
    }

    auto keyboardListener = EventListenerKeyboard::create();
    keyboardListener->onKeyPressed = CC_CALLBACK_2(GameScene::onKeyPressed, this);
    keyboardListener->onKeyReleased = CC_CALLBACK_2(GameScene::onKeyReleased, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);
    
    auto listener = EventListenerMouse::create();
    listener->onMouseMove = CC_CALLBACK_1(GameScene::onMouseMove, this);
    listener->onMouseDown = CC_CALLBACK_1(GameScene::onMouseDown, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);


    this->scheduleUpdate();

    return true;
}

void GameScene::update(float aDelta)
{
    if (mSpaceship)
    {
        const int speed = 200;
        const auto visibleSize = Director::getInstance()->getVisibleSize();

        Vec2 position = mSpaceship->getPosition();
        auto bbox = mSpaceship->getBoundingBox();

        if (position.y + speed * aDelta <= visibleSize.height - bbox.size.height / 2)
            if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_W) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_UP_ARROW))
                position.y += speed * aDelta;
        if (position.y - speed * aDelta >= bbox.size.height / 2)
            if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_S) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_DOWN_ARROW))
                position.y -= speed * aDelta;
        if (position.x - speed * aDelta >= bbox.size.width / 2)
            if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_A) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_LEFT_ARROW))
                position.x -= speed * aDelta;
        if (position.x + speed * aDelta <= visibleSize.width - bbox.size.width / 2)
            if (mPressedKeys.count(EventKeyboard::KeyCode::KEY_D) || mPressedKeys.count(EventKeyboard::KeyCode::KEY_RIGHT_ARROW))
                position.x += speed * aDelta;

        mSpaceship->setPosition(position);

        adjustSpaceshipRotation();
    }
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
    Vec2 direction = mMousePosition - mSpaceship->getPosition();
    float angle = CC_RADIANS_TO_DEGREES(atan2(direction.y, direction.x));
    // due to spaceship head looking up, we have to subtract 90 degrees
    angle -= 90;
    mSpaceship->setRotation(-angle);
}

void GameScene::onMouseDown(EventMouse* aEvent)
{
    if (aEvent && aEvent->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT && mIsCanShoot)
    {
        mIsCanShoot = false;
        shootBullet(aEvent->getLocationInView());
        this->scheduleOnce([this](float) { mIsCanShoot = true; }, mBulletCooldown, "fireCooldown");
    }
}

void GameScene::shootBullet(Vec2 target)
{
    if (mSpaceship)
    {
        auto bullet = Sprite::create("missile.png");
        bullet->setPosition(mSpaceship->getPosition());
        bullet->setRotation(mSpaceship->getRotation());
        this->addChild(bullet);

        Vec2 direction = target - bullet->getPosition();
        direction.normalize();

        float speed = 500.0f;
        Vec2 velocity = direction * speed;

        auto moveAction = MoveBy::create(1.0f, velocity);
        auto removeAction = RemoveSelf::create();
        bullet->runAction(Sequence::create(moveAction, removeAction, nullptr));
    }
}
