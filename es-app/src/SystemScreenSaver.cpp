#include "SystemScreenSaver.h"

#include "math/Transform4x4f.h"
#include "renderers/Renderer.h"
#include "PowerSaver.h"
#include "Settings.h"

SystemScreenSaver::SystemScreenSaver(Window* window) :
	mWindow(window),
	mActive(false)
{
	mWindow->setScreenSaver(this);
}

SystemScreenSaver::~SystemScreenSaver()
{
}

bool SystemScreenSaver::allowSleep()
{
	return true;
}

bool SystemScreenSaver::isScreenSaverActive()
{
	return mActive;
}

void SystemScreenSaver::startScreenSaver(SystemData* /*system*/)
{
	mActive = true;
	PowerSaver::runningScreenSaver(true);
}

void SystemScreenSaver::stopScreenSaver(bool /*toResume*/)
{
	mActive = false;
	PowerSaver::runningScreenSaver(false);
}

void SystemScreenSaver::renderScreenSaver()
{
	if (!mActive)
		return;

	// "black" = tela preta; qualquer outro valor = escurecer
	bool black = Settings::getInstance()->getString("ScreenSaverBehavior") == "black";
	unsigned int color = black ? 0x000000FF : 0x000000A0;
	Renderer::setMatrix(Transform4x4f::Identity());
	Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), color, color);
}

void SystemScreenSaver::update(int /*deltaTime*/)
{
}
