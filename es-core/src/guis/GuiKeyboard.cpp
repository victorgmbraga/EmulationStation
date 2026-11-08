#include "guis/GuiKeyboard.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "InputConfig.h"
#include "Window.h"

// cores da interface BTV9 (as mesmas do menu)
#define KB_TEXT          0xBCBEBCFF
#define KB_TEXT_SELECTED 0xF7FBF7FF
#define KB_KEY           0x161816FF
#define KB_KEY_SELECTED  0x3A3D3AFF
#define KB_FIELD         0x000000FF
#define KB_FIELD_BORDER  0x5C5E5CFF

#define MOVE_DELAY_MS  350
#define MOVE_REPEAT_MS 90

static const char* PAGES[3][4] = {
	{ "1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm.-_" },
	{ "1234567890", "QWERTYUIOP", "ASDFGHJKL@", "ZXCVBNM.-_" },
	{ "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>/?", "`~" },
};

GuiKeyboard::GuiKeyboard(Window* window, const std::string& title, const std::string& initValue,
	const std::function<void(const std::string&)>& okCallback, size_t maxLength,
	const std::function<void()>& cancelCallback)
	: GuiComponent(window), mBackground(window, ":/frame.png"),
	mTitle(window, title, Font::get(FONT_SIZE_LARGE), KB_TEXT_SELECTED, ALIGN_CENTER),
	mValue(window, "", Font::get(FONT_SIZE_MEDIUM), KB_TEXT_SELECTED, ALIGN_LEFT),
	mFont(Font::get(FONT_SIZE_MEDIUM)), mRow(1), mCol(0), mPage(0), mText(initValue),
	mMaxLength(maxLength), mOkCallback(okCallback), mCancelCallback(cancelCallback), mMoveDx(0), mMoveDy(0), mMoveTime(0)
{
	const float fh = mFont->getHeight();
	mCellH = Math::round(fh * 1.5f) + 2;
	mCellW = Math::round(mCellH * 1.4f);
	mGap = Renderer::isPixelMode() ? 2 : Math::round(mCellH * 0.1f);
	mPad = Renderer::isPixelMode() ? 8 : Math::round(mCellH * 0.4f);

	const float gridW = 10 * mCellW + 9 * mGap;
	const float titleH = mTitle.getFont()->getHeight();
	mFieldY = titleH + mGap * 2;
	mKeysY = mFieldY + mCellH + mPad;
	const float keysH = 5 * mCellH + 4 * mGap;

	setSize(gridW + mPad * 2, mKeysY + keysH + mPad);
	setPosition(Math::round((Renderer::getScreenWidth() - mSize.x()) / 2), Math::round((Renderer::getScreenHeight() - mSize.y()) / 2));

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	mTitle.setSize(mSize.x(), titleH);
	mTitle.setPosition(0, 0);
	addChild(&mBackground);
	addChild(&mTitle);

	mValue.setPosition(mPad + 4, mFieldY + Math::round((mCellH - fh) / 2));

	buildKeys();
	updateValue();
}

void GuiKeyboard::buildKeys()
{
	mRows.clear();
	const float gridW = 10 * mCellW + 9 * mGap;
	const float fh = mFont->getHeight();

	auto makeCaches = [this, fh](Key& k) {
		float tx = Math::round(k.x + (k.w - mFont->sizeText(k.label).x()) / 2);
		float ty = Math::round(k.y + (k.h - fh) / 2);
		k.normal = std::shared_ptr<TextCache>(mFont->buildTextCache(k.label, tx, ty, KB_TEXT));
		k.selected = std::shared_ptr<TextCache>(mFont->buildTextCache(k.label, tx, ty, KB_TEXT_SELECTED));
	};

	// quatro linhas de caracteres, centralizadas
	for(int r = 0; r < 4; r++)
	{
		const std::string chars = PAGES[mPage][r];
		const int n = (int)chars.size();
		const float rowW = n * mCellW + (n - 1) * mGap;
		float x = mPad + Math::round((gridW - rowW) / 2);
		const float y = mKeysY + r * (mCellH + mGap);

		std::vector<Key> row;
		for(int i = 0; i < n; i++)
		{
			Key k { std::string(1, chars[i]), CHAR, x, y, mCellW, mCellH, nullptr, nullptr };
			makeCaches(k);
			row.push_back(k);
			x += mCellW + mGap;
		}
		mRows.push_back(row);
	}

	// linha de ações
	const std::vector< std::pair<std::string, Action> > actions = {
		{ mPage == 0 ? "ABC" : (mPage == 1 ? "abc" : "ABC"), CASE },
		{ mPage == 2 ? "abc" : "#+=", SYMBOLS },
		{ "ESPAÇO", SPACE },
		{ "APAGAR", DELETE },
		{ "OK", OK },
	};
	const float aw = Math::round((gridW - 4 * mGap) / 5);
	const float y = mKeysY + 4 * (mCellH + mGap);
	std::vector<Key> row;
	float x = mPad;
	for(auto& a : actions)
	{
		Key k { a.first, a.second, x, y, aw, mCellH, nullptr, nullptr };
		makeCaches(k);
		row.push_back(k);
		x += aw + mGap;
	}
	mRows.push_back(row);

	mRow = Math::clamp(mRow, 0, (int)mRows.size() - 1);
	mCol = Math::clamp(mCol, 0, (int)mRows[mRow].size() - 1);
}

void GuiKeyboard::move(int dx, int dy)
{
	if(dx)
	{
		const int n = (int)mRows[mRow].size();
		mCol = (mCol + dx + n) % n;
	}
	if(dy)
	{
		// vai para a tecla mais próxima (na horizontal) da linha de cima/baixo
		const Key& cur = mRows[mRow][mCol];
		const float cx = cur.x + cur.w / 2;
		const int r = (mRow + dy + (int)mRows.size()) % (int)mRows.size();
		int best = 0;
		float bestDist = 1e9f;
		for(int i = 0; i < (int)mRows[r].size(); i++)
		{
			const Key& k = mRows[r][i];
			float d = (k.x + k.w / 2) - cx;
			if(d < 0) d = -d;
			if(d < bestDist) { bestDist = d; best = i; }
		}
		mRow = r;
		mCol = best;
	}
}

void GuiKeyboard::typeText(const std::string& s)
{
	if(mText.size() + s.size() <= mMaxLength)
	{
		mText += s;
		updateValue();
	}
}

void GuiKeyboard::backspace()
{
	if(mText.empty())
		return;
	// apaga um caractere inteiro (UTF-8)
	size_t i = mText.size() - 1;
	while(i > 0 && ((unsigned char)mText[i] & 0xC0) == 0x80)
		i--;
	mText.erase(i);
	updateValue();
}

void GuiKeyboard::updateValue()
{
	// mostra o fim do texto quando ele não cabe no campo
	const float maxW = mSize.x() - mPad * 2 - 8;
	std::string shown = mText + "_";
	while(shown.size() > 1 && mFont->sizeText(shown).x() > maxW)
	{
		size_t n = 1;
		while(n < shown.size() && ((unsigned char)shown[n] & 0xC0) == 0x80)
			n++;
		shown.erase(0, n);
	}
	mValue.setText(shown);
}

void GuiKeyboard::press(const Key& key)
{
	switch(key.action)
	{
	case CHAR:    typeText(key.label); break;
	case SPACE:   typeText(" "); break;
	case DELETE:  backspace(); break;
	case OK:      confirm(); break;
	case CASE:    mPage = (mPage == 0) ? 1 : (mPage == 1 ? 0 : 1); buildKeys(); break;
	case SYMBOLS: mPage = (mPage == 2) ? 0 : 2; buildKeys(); break;
	}
}

void GuiKeyboard::confirm()
{
	auto callback = mOkCallback;
	std::string text = mText;
	delete this;
	if(callback)
		callback(text);
}

void GuiKeyboard::cancel()
{
	auto callback = mCancelCallback;
	delete this;
	if(callback)
		callback();
}

bool GuiKeyboard::input(InputConfig* config, Input input)
{
	const bool pressed = input.value != 0;

	struct { const char* name; int dx, dy; } dirs[] = {
		{ "up", 0, -1 }, { "down", 0, 1 }, { "left", -1, 0 }, { "right", 1, 0 } };
	for(auto& d : dirs)
	{
		if(config->isMappedLike(d.name, input))
		{
			if(pressed)
			{
				mMoveDx = d.dx; mMoveDy = d.dy; mMoveTime = -MOVE_DELAY_MS;
				move(d.dx, d.dy);
			}else{
				// soltar: no hat/eixo o valor 0 "casa" com todas as direções, então para qualquer repetição
				mMoveDx = mMoveDy = 0;
			}
			return true;
		}
	}

	if(!pressed)
		return true;

	if(config->isMappedTo("a", input))
	{
		Key key = mRows[mRow][mCol]; // cópia: press() pode refazer as teclas ou fechar o teclado
		press(key);
	}
	else if(config->isMappedTo("b", input))
	{
		if(mText.empty())
			cancel(); // campo vazio: B sai
		else
			backspace();
	}
	else if(config->isMappedTo("x", input))
		typeText(" ");
	else if(config->isMappedTo("y", input))
	{
		mPage = (mPage == 0) ? 1 : 0;
		buildKeys();
	}
	else if(config->isMappedTo("start", input))
		confirm();
	else if(config->isMappedTo("select", input))
		cancel();

	return true;
}

void GuiKeyboard::update(int deltaTime)
{
	if(mMoveDx || mMoveDy)
	{
		mMoveTime += deltaTime;
		while(mMoveTime >= MOVE_REPEAT_MS)
		{
			move(mMoveDx, mMoveDy);
			mMoveTime -= MOVE_REPEAT_MS;
		}
	}
	GuiComponent::update(deltaTime);
}

void GuiKeyboard::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	renderChildren(trans); // moldura e título

	Renderer::setMatrix(trans);

	// campo de texto com borda
	const float fw = mSize.x() - mPad * 2;
	Renderer::drawRect(mPad, mFieldY, fw, mCellH, KB_FIELD_BORDER, KB_FIELD_BORDER);
	Renderer::drawRect(mPad + 1, mFieldY + 1, fw - 2, mCellH - 2, KB_FIELD, KB_FIELD);
	mValue.render(trans);

	Renderer::setMatrix(trans);
	for(int r = 0; r < (int)mRows.size(); r++)
	{
		for(int c = 0; c < (int)mRows[r].size(); c++)
		{
			const Key& k = mRows[r][c];
			const bool sel = (r == mRow && c == mCol);
			Renderer::drawRect(k.x, k.y, k.w, k.h, sel ? KB_KEY_SELECTED : KB_KEY, sel ? KB_KEY_SELECTED : KB_KEY);
			mFont->renderTextCache(sel ? k.selected.get() : k.normal.get());
		}
	}
}

std::vector<HelpPrompt> GuiKeyboard::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down/left/right", "MOVER"));
	prompts.push_back(HelpPrompt("a", "DIGITAR"));
	prompts.push_back(HelpPrompt("b", "APAGAR"));
	prompts.push_back(HelpPrompt("x", "ESPAÇO"));
	prompts.push_back(HelpPrompt("y", "MAIUSC."));
	prompts.push_back(HelpPrompt("start", "OK"));
	prompts.push_back(HelpPrompt("select", "CANCELAR"));
	return prompts;
}
