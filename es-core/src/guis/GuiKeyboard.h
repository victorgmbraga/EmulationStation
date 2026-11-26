#pragma once
#ifndef ES_CORE_GUIS_GUI_KEYBOARD_H
#define ES_CORE_GUIS_GUI_KEYBOARD_H

// BTV9: teclado na tela para digitar com o controle (ex.: senha do Wi-Fi).
// Direcional move, A digita, B apaga (com o campo vazio, fecha), X espaço, Y maiúsculas,
// START confirma, SELECT cancela.

#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "GuiComponent.h"
#include <functional>
#include <memory>

class GuiKeyboard : public GuiComponent
{
public:
	GuiKeyboard(Window* window, const std::string& title, const std::string& initValue,
		const std::function<void(const std::string&)>& okCallback, size_t maxLength = 63,
		const std::function<void()>& cancelCallback = nullptr);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	enum Action { CHAR, CASE, SYMBOLS, SPACE, DELETE, OK };
	struct Key
	{
		std::string label;
		Action action;
		float x, y, w, h;
		std::shared_ptr<TextCache> normal, selected;
	};

	void buildKeys();
	void move(int dx, int dy);
	void press(const Key& key);
	void typeText(const std::string& s);
	void backspace();
	void confirm();
	void cancel();
	void updateValue();

	NinePatchComponent mBackground;
	TextComponent mTitle;
	TextComponent mValue;
	std::shared_ptr<Font> mFont;

	std::vector< std::vector<Key> > mRows;
	int mRow, mCol;
	int mPage; // 0 minúsculas, 1 maiúsculas, 2 símbolos
	std::string mText;
	size_t mMaxLength;
	std::function<void(const std::string&)> mOkCallback;
	std::function<void()> mCancelCallback;

	float mPad, mCellW, mCellH, mGap, mFieldY, mKeysY;

	// repetição ao segurar o direcional
	int mMoveDx, mMoveDy, mMoveTime;
};

#endif // ES_CORE_GUIS_GUI_KEYBOARD_H
