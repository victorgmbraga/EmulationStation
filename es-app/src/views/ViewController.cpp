#include "views/ViewController.h"

#include "animations/Animation.h"
#include "animations/LambdaAnimation.h"
#include "animations/LaunchAnimation.h"
#include "guis/GuiMenu.h"
#include "views/gamelist/BasicGameListView.h"
#include "views/gamelist/IGameListView.h"
#include "views/SystemView.h"
#include "views/UIModeController.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Scripting.h"
#include "Settings.h"
#include "SystemData.h"
#include "Window.h"

ViewController* ViewController::sInstance = NULL;

ViewController* ViewController::get()
{
	assert(sInstance);
	return sInstance;
}

void ViewController::init(Window* window)
{
	assert(!sInstance);
	sInstance = new ViewController(window);
}

ViewController::ViewController(Window* window)
	: GuiComponent(window), mCurrentView(nullptr), mCamera(Transform4x4f::Identity()), mFadeOpacity(0), mLockInput(false)
{
	mState.viewing = NOTHING;
}

ViewController::~ViewController()
{
	assert(sInstance == this);
	sInstance = NULL;
}

void ViewController::goToStart()
{
	// If specific system is requested, go directly to the game list
	auto requestedSystem = Settings::getInstance()->getString("StartupSystem");
	if("" != requestedSystem && "retropie" != requestedSystem)
	{
		for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++){
			if ((*it)->getName() == requestedSystem)
			{
				goToGameList(*it);
				Scripting::fireEvent("system-select", requestedSystem, "requestedsystem");
				FileData* cursor = getGameListView(*it)->getCursor();
				if (cursor != NULL)
				{
					Scripting::fireEvent("game-select", requestedSystem, cursor->getPath(), cursor->getName(), "requestedgame");
				}
				else
				{
					Scripting::fireEvent("game-select", "NULL", "NULL", "NULL", "requestedgame");
				}
				return;
			}
		}

		// Requested system doesn't exist
		Settings::getInstance()->setString("StartupSystem", "");
	}
	goToSystemView(SystemData::sSystemVector.at(0));
	Scripting::fireEvent("system-select", SystemData::sSystemVector.at(0)->getName(), "gotostart");
}

void ViewController::ReloadAndGoToStart()
{
	mWindow->renderLoadingScreen("Loading...");
	ViewController::get()->reloadAll();
	ViewController::get()->goToStart();
}

int ViewController::getSystemId(SystemData* system)
{
	std::vector<SystemData*>& sysVec = SystemData::sSystemVector;
	return (int)(std::find(sysVec.cbegin(), sysVec.cend(), system) - sysVec.cbegin());
}

void ViewController::goToSystemView(SystemData* system)
{
	// Tell any current view it's about to be hidden
	if (mCurrentView)
	{
		mCurrentView->onHide();
	}

	mState.viewing = SYSTEM_SELECT;
	mState.system = system;

	auto systemList = getSystemListView();
	systemList->setPosition(getSystemId(system) * (float)Renderer::getScreenWidth(), systemList->getPosition().y());

	systemList->goToSystem(system, false);
	mCurrentView = systemList;
	mCurrentView->onShow();
	PowerSaver::setState(true);

	playViewTransition();
}

void ViewController::goToNextGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getNext());
}

void ViewController::goToPrevGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getPrev());
}

void ViewController::goToGameList(SystemData* system)
{
	if(mState.viewing == SYSTEM_SELECT)
	{
		// move system list
		auto sysList = getSystemListView();
		float offX = sysList->getPosition().x();
		int sysId = getSystemId(system);
		sysList->setPosition(sysId * (float)Renderer::getScreenWidth(), sysList->getPosition().y());
		offX = sysList->getPosition().x() - offX;
		mCamera.translation().x() -= offX;
	}

	mState.viewing = GAME_LIST;
	mState.system = system;

	if (mCurrentView)
	{
		mCurrentView->onHide();
	}
	mCurrentView = getGameListView(system);
	if (mCurrentView)
	{
		mCurrentView->onShow();
	}
	playViewTransition();
}

void ViewController::playViewTransition()
{
	Vector3f target(Vector3f::Zero());
	if(mCurrentView)
		target = mCurrentView->getPosition();

	// no need to animate, we're not going anywhere (probably goToNextGamelist() or goToPrevGamelist() when there's only 1 system)
	if(target == -mCamera.translation() && !isAnimationPlaying(0))
		return;

	// BTV9: transição sempre instantânea
	setAnimation(new LambdaAnimation(
		[this, target](float /*t*/)
	{
		this->mCamera.translation() = -target;
	}, 1));
	updateHelpPrompts();
}

void ViewController::onFileChanged(FileData* file, FileChangeType change)
{
	auto it = mGameListViews.find(file->getSystem());
	if(it != mGameListViews.cend())
		it->second->onFileChanged(file, change);
}

void ViewController::launch(FileData* game, Vector3f center)
{
	if(game->getType() != GAME)
	{
		LOG(LogError) << "tried to launch something that isn't a game";
		return;
	}

	// Hide the current view
	if (mCurrentView)
		mCurrentView->onHide();

	Transform4x4f origCamera = mCamera;
	origCamera.translation() = -mCurrentView->getPosition();

	center += mCurrentView->getPosition();
	stopAnimation(1); // make sure the fade in isn't still playing
	mWindow->stopInfoPopup(); // make sure we disable any existing info popup
	mLockInput = true;

	// BTV9: transição sempre instantânea
	setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 10), 0, [this, origCamera, center, game]
	{
		game->launchGame(mWindow);
		mCamera = origCamera;
		setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 10), 0, [this, game] { mLockInput = false; }, true);
		this->onFileChanged(game, FILE_METADATA_CHANGED);
		if (mCurrentView) {
			this->getGameListView(game->getSystem())->setCursor(game, true);
			mCurrentView->onShow();
		}
	});
}

void ViewController::removeGameListView(SystemData* system)
{
	//if we already made one, return that one
	auto exists = mGameListViews.find(system);
	if(exists != mGameListViews.cend())
	{
		exists->second.reset();
		mGameListViews.erase(system);
	}
}

std::shared_ptr<IGameListView> ViewController::getGameListView(SystemData* system)
{
	//if we already made one, return that one
	auto exists = mGameListViews.find(system);
	if(exists != mGameListViews.cend())
		return exists->second;

	system->getIndex()->setUIModeFilters();
	//if we didn't, make it, remember it, and return it
	std::shared_ptr<IGameListView> view;

	// BTV9: só existe a lista básica (sem imagens nem vídeos)
	view = std::shared_ptr<IGameListView>(new BasicGameListView(mWindow, system->getRootFolder()));

	view->setTheme(system->getTheme());

	std::vector<SystemData*>& sysVec = SystemData::sSystemVector;
	int id = (int)(std::find(sysVec.cbegin(), sysVec.cend(), system) - sysVec.cbegin());
	view->setPosition(id * (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight() * 2);

	addChild(view.get());

	mGameListViews[system] = view;
	return view;
}

std::shared_ptr<SystemView> ViewController::getSystemListView()
{
	//if we already made one, return that one
	if(mSystemListView)
		return mSystemListView;

	mSystemListView = std::shared_ptr<SystemView>(new SystemView(mWindow));
	addChild(mSystemListView.get());
	mSystemListView->setPosition(0, (float)Renderer::getScreenHeight());
	return mSystemListView;
}


bool ViewController::input(InputConfig* config, Input input)
{
	if(mLockInput)
		return true;

	// open menu
	if(!(UIModeController::getInstance()->isUIModeKid() && Settings::getInstance()->getBool("DisableKidStartMenu")) && config->isMappedTo("start", input) && input.value != 0)
	{
		// open menu
		mWindow->pushGui(new GuiMenu(mWindow));
		return true;
	}

	if(UIModeController::getInstance()->listen(config, input))  // check if UI mode has changed due to passphrase completion
	{
		return true;
	}

	if(mCurrentView)
		return mCurrentView->input(config, input);

	return false;
}

void ViewController::update(int deltaTime)
{
	if(mCurrentView)
	{
		mCurrentView->update(deltaTime);
	}

	updateSelf(deltaTime);
}

void ViewController::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = mCamera * parentTrans;
	Transform4x4f transInverse;
	transInverse.invert(trans);

	// camera position, position + size
	Vector3f viewStart = transInverse.translation();
	Vector3f viewEnd = transInverse * Vector3f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight(), 0);

	// Keep track of UI mode changes.
	UIModeController::getInstance()->monitorUIMode();

	// draw systemview
	getSystemListView()->render(trans);

	// draw gamelists
	for(auto it = mGameListViews.cbegin(); it != mGameListViews.cend(); it++)
	{
		// clipping
		Vector3f guiStart = it->second->getPosition();
		Vector3f guiEnd = it->second->getPosition() + Vector3f(it->second->getSize().x(), it->second->getSize().y(), 0);

		if(guiEnd.x() >= viewStart.x() && guiEnd.y() >= viewStart.y() &&
			guiStart.x() <= viewEnd.x() && guiStart.y() <= viewEnd.y())
			it->second->render(trans);
	}

	if(mWindow->peekGui() == this)
		mWindow->renderHelpPromptsEarly();

	// fade out
	if(mFadeOpacity)
	{
		unsigned int fadeColor = 0x00000000 | (unsigned char)(mFadeOpacity * 255);
		Renderer::setMatrix(parentTrans);
		Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), fadeColor, fadeColor);
	}
}

void ViewController::preload()
{
	int i = 1;
	int max = SystemData::sSystemVector.size() + 1;

	bool splash = Settings::getInstance()->getBool("SplashScreen");
	if (splash)
		mWindow->renderLoadingScreen("Preloading UI", (float)i / (float)max);

	for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		if (splash)
		{
			i++;
			mWindow->renderLoadingScreen("Preloading UI", (float)i / (float)max);
		}

		(*it)->getIndex()->resetFilters();
		getGameListView(*it);
	}
}

void ViewController::reloadGameListView(IGameListView* view, bool reloadTheme)
{
	for(auto it = mGameListViews.cbegin(); it != mGameListViews.cend(); it++)
	{
		if(it->second.get() == view)
		{
			bool isCurrent = (mCurrentView == it->second);
			SystemData* system = it->first;
			FileData* cursor = view->getCursor();
			int viewportTop = view->getViewportTop();
			mGameListViews.erase(it);

			if(reloadTheme)
				system->loadTheme();
			system->getIndex()->setUIModeFilters();
			std::shared_ptr<IGameListView> newView = getGameListView(system);

			// to counter having come from a placeholder
			if (!cursor->isPlaceHolder()) {
				newView->setCursor(cursor);
				newView->setViewportTop(viewportTop);
			}
			if(isCurrent)
				mCurrentView = newView;

			break;
		}
	}
	// Redisplay the current view
	if (mCurrentView)
		mCurrentView->onShow();

}

void ViewController::reloadAll(bool themeChanged)
{
	// clear all gamelistviews
	std::map<SystemData*, FileData*> cursorMap;
	std::map<SystemData*, int> viewportTopMap;
	for(auto it = mGameListViews.cbegin(); it != mGameListViews.cend(); it++)
	{
		cursorMap[it->first] = it->second->getCursor();
		viewportTopMap[it->first] = it->second->getViewportTop();
	}
	mGameListViews.clear();

	// load themes, create gamelistviews and reset filters
	for(auto it = cursorMap.cbegin(); it != cursorMap.cend(); it++)
	{
		it->first->loadTheme();
		it->first->getIndex()->resetFilters();
		getGameListView(it->first)->setCursor(it->second);
	}

	if(!themeChanged || !Settings::getInstance()->getBool("UseFullscreenPaging"))
	{
		// restore index of first list item on display
		for(auto it = viewportTopMap.cbegin(); it != viewportTopMap.cend(); it++)
		{
			getGameListView(it->first)->setViewportTop(it->second);
		}
	}

	// Rebuild SystemListView
	mSystemListView.reset();
	getSystemListView();

	// update mCurrentView since the pointers changed
	if(mState.viewing == GAME_LIST)
	{
		mCurrentView = getGameListView(mState.getSystem());
	}else if(mState.viewing == SYSTEM_SELECT)
	{
		SystemData* system = mState.getSystem();
		goToSystemView(SystemData::sSystemVector.front());
		mSystemListView->goToSystem(system, false);
		mCurrentView = mSystemListView;
	}else{
		goToSystemView(SystemData::sSystemVector.front());
	}

	updateHelpPrompts();
}

std::vector<HelpPrompt> ViewController::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	if(!mCurrentView)
		return prompts;

	prompts = mCurrentView->getHelpPrompts();
	if(!(UIModeController::getInstance()->isUIModeKid() && Settings::getInstance()->getBool("DisableKidStartMenu")))
		prompts.push_back(HelpPrompt("start", "menu"));

	return prompts;
}

HelpStyle ViewController::getHelpStyle()
{
	if(!mCurrentView)
		return GuiComponent::getHelpStyle();

	return mCurrentView->getHelpStyle();
}
