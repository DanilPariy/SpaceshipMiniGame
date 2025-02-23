#include "MainMenuScene.h"

#include "GameScene.h"

MainMenuScene::MainMenuScene()
{

}

MainMenuScene::~MainMenuScene()
{

}

bool MainMenuScene::init()
{
    if (!Scene::init())
    {
        return false;
    }

    auto visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    auto titleLabel = Label::createWithTTF("Spaceship MiniGame", "fonts/Marker Felt.ttf", 48);
    titleLabel->setPosition(Vec2(origin.x + visibleSize.width / 2,
        origin.y + visibleSize.height - titleLabel->getContentSize().height));
    this->addChild(titleLabel);

    auto startItem = MenuItemLabel::create(Label::createWithTTF("Start Game", "fonts/Marker Felt.ttf", 36),
        [](Ref* sender) {
            Director::getInstance()->replaceScene(GameScene::createScene());
        });

    auto exitItem = MenuItemLabel::create(Label::createWithTTF("Exit", "fonts/Marker Felt.ttf", 36),
        [](Ref* sender) {
            Director::getInstance()->end();
        });

    auto menu = Menu::create(startItem, exitItem, nullptr);
    menu->alignItemsVerticallyWithPadding(20.0f);
    menu->setPosition(Vec2(origin.x + visibleSize.width / 2, origin.y + visibleSize.height / 2));
    this->addChild(menu);

    return true;
}