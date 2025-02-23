#ifndef __MAIN_MENU_SCENE_H__
#define __MAIN_MENU_SCENE_H__

#include "cocos2d.h"

USING_NS_CC;

class MainMenuScene
    : public Scene
{
public:
    MainMenuScene();
    virtual ~MainMenuScene();

    virtual bool init() override;

    CREATE_FUNC(MainMenuScene);
};

#endif // __MAIN_MENU_SCENE_H__
