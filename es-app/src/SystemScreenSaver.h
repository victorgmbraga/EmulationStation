#pragma once
#ifndef ES_APP_SYSTEM_SCREEN_SAVER_H
#define ES_APP_SYSTEM_SCREEN_SAVER_H

#include "Window.h"

// Proteção de tela do BTV9: só "dim" (escurecer) ou "black" (tela preta).
class SystemScreenSaver : public Window::ScreenSaver
{
public:
	SystemScreenSaver(Window* window);
	virtual ~SystemScreenSaver();

	virtual void startScreenSaver(SystemData* system=NULL);
	virtual void stopScreenSaver(bool toResume=false);
	virtual void renderScreenSaver();
	virtual bool allowSleep();
	virtual void update(int deltaTime);
	virtual bool isScreenSaverActive();

private:
	Window*	mWindow;
	bool	mActive;
};

#endif // ES_APP_SYSTEM_SCREEN_SAVER_H
