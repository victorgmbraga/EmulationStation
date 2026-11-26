#include "guis/GuiBusy.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "resources/Font.h"

GuiBusy::GuiBusy(Window* window, const std::string& text,
	const std::function<std::string()>& task, const std::function<void(const std::string&)>& done)
	: GuiComponent(window), mBackground(window, ":/frame.png"),
	mText(window, text + "...", Font::get(FONT_SIZE_MEDIUM), 0xBCBEBCFF, ALIGN_CENTER),
	mBaseText(text), mTime(0), mFinished(false), mDone(done)
{
	const auto font = Font::get(FONT_SIZE_MEDIUM);
	float w = Math::max(font->sizeText(text + "...").x() + 48.0f, Renderer::getScreenWidth() * 0.3f);
	float h = font->getHeight() * 3.0f;
	setSize(w, h);
	setPosition(Math::round((Renderer::getScreenWidth() - w) / 2), Math::round((Renderer::getScreenHeight() - h) / 2));

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	mText.setSize(w, h);
	addChild(&mBackground);
	addChild(&mText);

	mThread = std::thread([this, task] {
		mResult = task();
		mFinished = true;
	});
}

GuiBusy::~GuiBusy()
{
	if(mThread.joinable())
		mThread.join();
}

void GuiBusy::update(int deltaTime)
{
	// reticências animadas para mostrar que não travou
	mTime += deltaTime;
	int dots = (mTime / 400) % 4;
	mText.setText(mBaseText + std::string(dots, '.') + std::string(3 - dots, ' '));

	if(mFinished)
	{
		mThread.join();
		auto done = mDone;
		std::string result = mResult;
		delete this;
		if(done)
			done(result);
	}
}
