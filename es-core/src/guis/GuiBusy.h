#pragma once
#ifndef ES_CORE_GUIS_GUI_BUSY_H
#define ES_CORE_GUIS_GUI_BUSY_H

// BTV9: caixa "aguarde..." que roda uma tarefa demorada (ex.: buscar redes Wi-Fi) em segundo plano,
// sem travar a interface. Ao terminar, fecha e chama done() com o resultado da tarefa.

#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "GuiComponent.h"
#include <atomic>
#include <functional>
#include <thread>

class GuiBusy : public GuiComponent
{
public:
	GuiBusy(Window* window, const std::string& text,
		const std::function<std::string()>& task, const std::function<void(const std::string&)>& done);
	~GuiBusy();

	bool input(InputConfig* config, Input input) override { return true; } // ignora o controle enquanto espera
	void update(int deltaTime) override;
	std::vector<HelpPrompt> getHelpPrompts() override { return std::vector<HelpPrompt>(); }

private:
	NinePatchComponent mBackground;
	TextComponent mText;
	std::string mBaseText;
	int mTime;

	std::thread mThread;
	std::atomic<bool> mFinished;
	std::string mResult;
	std::function<void(const std::string&)> mDone;
};

#endif // ES_CORE_GUIS_GUI_BUSY_H
