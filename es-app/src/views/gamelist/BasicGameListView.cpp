#include "views/gamelist/BasicGameListView.h"

#include "renderers/Renderer.h"
#include "utils/FileSystemUtil.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Settings.h"
#include "SystemData.h"
#include <algorithm>
#include <cmath>

BasicGameListView::BasicGameListView(Window* window, FileData* root)
	: ISimpleGameListView(window, root), mList(window), mImage(window, false, false),
	  mImageBoxPos(0, 0), mImageBoxSize(0, 0)
{
	mList.setSize(mSize.x(), mSize.y() * 0.8f);
	mList.setPosition(0, mSize.y() * 0.2f);
	mList.setDefaultZIndex(20);
	mList.setFavoriteIndicatorCallback([](FileData* file) {
		return file != nullptr && file->getType() == GAME && file->metadata.get("favorite") == "true";
	});
	addChild(&mList);

	mImage.setDefaultZIndex(30);
	mImage.setVisible(false);
	addChild(&mImage);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateGameImage(); });

	populateList(root->getChildrenListToDisplay());
}

// BTV9: a imagem só aparece se o tema definir md_image (pos e size = área disponível). A escala é inteira
// (ou 1/n quando a imagem é maior que a área) e a posição cai em pixel inteiro: no modo pixel, cada pixel
// da tela do jogo vira um bloco do mesmo tamanho.
void BasicGameListView::updateGameImage()
{
	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? nullptr : mList.getSelected();
	std::string path;
	if(file && file->getType() == GAME && mImageBoxSize.x() > 0 && mImageBoxSize.y() > 0)
	{
		const std::string rom = file->getSourceFileData()->getPath();
		const std::string candidate = Utils::FileSystem::getParent(rom) + "/media/" + Utils::FileSystem::getStem(rom) + ".png";
		if(Utils::FileSystem::exists(candidate))
			path = candidate;
	}
	if(path == mImagePath)
		return;
	mImagePath = path;
	if(path.empty())
	{
		mImage.setVisible(false);
		return;
	}

	mImage.setImage(path);
	const Vector2i tex = mImage.getTextureSize();
	if(tex.x() <= 0 || tex.y() <= 0)
	{
		mImage.setVisible(false);
		return;
	}
	float scale;
	const float fit = std::min(mImageBoxSize.x() / tex.x(), mImageBoxSize.y() / tex.y());
	if(fit >= 1.0f)
		scale = std::floor(fit);
	else
		scale = 1.0f / std::ceil(1.0f / fit);
	const float w = std::floor(tex.x() * scale), h = std::floor(tex.y() * scale);
	mImage.setResize(w, h);
	mImage.setOrigin(0, 0);
	mImage.setPosition(std::floor(mImageBoxPos.x() + (mImageBoxSize.x() - w) / 2.0f),
	                   std::floor(mImageBoxPos.y() + (mImageBoxSize.y() - h) / 2.0f));
	mImage.setVisible(true);
}

void BasicGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	ISimpleGameListView::onThemeChanged(theme);
	using namespace ThemeFlags;
	mList.applyTheme(theme, getName(), "gamelist", ALL);

	mImageBoxSize = Vector2f(0, 0);
	const ThemeData::ThemeElement* elem = theme->getElement(getName(), "md_image", "image");
	if(elem && elem->has("pos") && elem->has("size"))
	{
		const Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		mImageBoxPos = elem->get<Vector2f>("pos") * screen;
		mImageBoxSize = elem->get<Vector2f>("size") * screen;
	}
	if(elem && elem->has("zIndex"))
		mImage.setZIndex(elem->get<float>("zIndex"));
	mImagePath = "-";   // força recarregar com a área nova
	updateGameImage();

	sortChildren();
}

void BasicGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	if(change == FILE_METADATA_CHANGED)
	{
		// might switch to a detailed view
		ViewController::get()->reloadGameListView(this);
		return;
	}

	ISimpleGameListView::onFileChanged(file, change);
}

void BasicGameListView::populateList(const std::vector<FileData*>& files)
{
	mList.clear();
	mHeaderText.setText(mRoot->getSystem()->getFullName());
	if (files.size() > 0)
	{
		for(auto it = files.cbegin(); it != files.cend(); it++)
		{
			mList.add((*it)->getName(), *it, ((*it)->getType() == FOLDER));
		}
	}
	else
	{
		addPlaceholder();
	}
}

FileData* BasicGameListView::getCursor()
{
	return mList.getSelected();
}

void BasicGameListView::setCursor(FileData* cursor, bool refreshListCursorPos)
{
	if (refreshListCursorPos)
		setViewportTop(mList.REFRESH_LIST_CURSOR_POS);

	bool notInList = !mList.setCursor(cursor);
	if(!refreshListCursorPos && notInList && !cursor->isPlaceHolder())
	{
		populateList(cursor->getParent()->getChildrenListToDisplay());
		// this extra call is needed iff a system has games organized in folders
		// and the cursor is focusing a game in a folder
		if (cursor->getParent()->getType() == FOLDER)
			mList.setCursor(cursor);

		// update our cursor stack in case our cursor just got set to some folder we weren't in before
		if(mCursorStack.empty() || mCursorStack.top() != cursor->getParent())
		{
			std::stack<FileData*> tmp;
			FileData* ptr = cursor->getParent();
			while(ptr && ptr != mRoot)
			{
				tmp.push(ptr);
				ptr = ptr->getParent();
			}

			// flip the stack and put it in mCursorStack
			mCursorStack = std::stack<FileData*>();
			while(!tmp.empty())
			{
				mCursorStack.push(tmp.top());
				tmp.pop();
			}
		}
	}
}

void BasicGameListView::setViewportTop(int index)
{
	mList.setViewportTop(index);
}


int BasicGameListView::getViewportTop()
{
	return mList.getViewportTop();
}

void BasicGameListView::addPlaceholder()
{
	// empty list - add a placeholder
	FileData* placeholder = new FileData(PLACEHOLDER, "<No Entries Found>", this->mRoot->getSystem()->getSystemEnvData(), this->mRoot->getSystem());
	mList.add(placeholder->getName(), placeholder, (placeholder->getType() == PLACEHOLDER));
}

std::string BasicGameListView::getQuickSystemSelectRightButton()
{
	return "right";
}

std::string BasicGameListView::getQuickSystemSelectLeftButton()
{
	return "left";
}

void BasicGameListView::launch(FileData* game)
{
	ViewController::get()->launch(game);
}

void BasicGameListView::remove(FileData *game, bool deleteFile, bool refreshView)
{
	if (deleteFile)
		Utils::FileSystem::removeFile(game->getPath());  // actually delete the file on the filesystem
	FileData* parent = game->getParent();
	if (getCursor() == game)                     // Select next element in list, or prev if none
	{
		std::vector<FileData*> siblings = parent->getChildrenListToDisplay();
		auto gameIter = std::find(siblings.cbegin(), siblings.cend(), game);
		unsigned int gamePos = (int)std::distance(siblings.cbegin(), gameIter);
		if (gameIter != siblings.cend())
		{
			if ((gamePos + 1) < siblings.size())
			{
				setCursor(siblings.at(gamePos + 1));
			} else if (gamePos > 1) {
				setCursor(siblings.at(gamePos - 1));
			}
		}
	}
	mList.remove(game);
	if(mList.size() == 0)
	{
		addPlaceholder();
	}
	delete game;                                 // remove before repopulating (removes from parent)
	if(refreshView)
		onFileChanged(parent, FILE_REMOVED);     // update the view, with game removed
}

std::vector<HelpPrompt> BasicGameListView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if(Settings::getInstance()->getBool("QuickSystemSelect"))
		prompts.push_back(HelpPrompt("left/right", "system"));
	prompts.push_back(HelpPrompt("up/down", "choose"));
	prompts.push_back(HelpPrompt("a", "launch"));
	prompts.push_back(HelpPrompt("b", "back"));
	if(!UIModeController::getInstance()->isUIModeKid())
		prompts.push_back(HelpPrompt("select", "options"));
	if(mRoot->getSystem()->isGameSystem() && !UIModeController::getInstance()->isUIModeKid())
	{
		std::string prompt = CollectionSystemManager::get()->getEditingCollection();
		prompts.push_back(HelpPrompt("y", prompt));
	}
	return prompts;
}

void BasicGameListView::onFocusLost() {
	mList.stopScrolling(true);
}
