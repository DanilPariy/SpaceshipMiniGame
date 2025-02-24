#include "GameOverLayer.h"

#include "MainMenuScene.h"

GameOverLayer::GameOverLayer()
{

}

GameOverLayer::~GameOverLayer()
{

}

GameOverLayer* GameOverLayer::create(int aScore, float aTime, bool aIsWin)
{
    GameOverLayer* ret = new (std::nothrow) GameOverLayer();
    if (ret && ret->init(aScore, aTime, aIsWin))
    {
        ret->autorelease();
        return ret;
    }
    else
    {
        delete ret;
        return nullptr;
    }
}

bool GameOverLayer::init(int aScore, float aTime, bool aIsWin)
{
    if (!Layer::init())
    {
        return false;
    }

    auto visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    auto blackout = LayerColor::create(Color4B(0, 0, 0, 180));
    this->addChild(blackout);

    std::string gameStatus = aIsWin ? "You Win!" : "Game Over";
    auto statusLabel = Label::createWithTTF(gameStatus, "fonts/Marker Felt.ttf", 48);
    statusLabel->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height * 0.8f));
    this->addChild(statusLabel);

    std::string scoreText = "Score: " + std::to_string(aScore);
    auto scoreLabel = Label::createWithTTF(scoreText, "fonts/Marker Felt.ttf", 36);
    scoreLabel->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height * 0.6f));
    this->addChild(scoreLabel);

    std::string timeText = StringUtils::format("Time: %.1f seconds", aTime);
    auto timeLabel = Label::createWithTTF(timeText, "fonts/Marker Felt.ttf", 36);
    timeLabel->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height * 0.5f));
    this->addChild(timeLabel);

    auto menuItem = MenuItemLabel::create(Label::createWithTTF("Return to Main Menu", "fonts/Marker Felt.ttf", 36),
        [](Ref* sender) {
            Director::getInstance()->replaceScene(MainMenuScene::create());
        });

    auto menu = Menu::create(menuItem, nullptr);
    menu->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height * 0.3f));
    this->addChild(menu);

    return true;
}