#pragma once
#ifndef ES_APP_GUIS_GUI_WIFI_H
#define ES_APP_GUIS_GUI_WIFI_H

// BTV9: menu de Wi-Fi. Lista as redes próximas, conecta (senha pelo teclado na tela)
// e esquece redes salvas. Os comandos vão para /opt/btv9/wifi.sh (root via sudo).

#include "components/MenuComponent.h"
#include "GuiComponent.h"

// BTV9: roda "sudo -n <script> <args>" sem shell e devolve a saída (usado também pela atualização)
std::string btv9RunRoot(const std::string& script, const std::vector<std::string>& args, const std::string& input = "");

struct WifiNetwork
{
	bool active;
	int signal;
	std::string security;
	bool saved;
	std::string ssid;
	std::string uuid; // só nas redes salvas
};

class GuiWifi : public GuiComponent
{
public:
	// busca as redes (com a caixa "aguarde") e abre a lista
	static void open(Window* window);
	// lista as redes salvas (para esquecer)
	static void openSaved(Window* window);

	GuiWifi(Window* window, const std::vector<WifiNetwork>& networks);
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

private:
	void select(WifiNetwork net);
	static void connect(Window* window, const std::string& ssid, const std::string& password, bool secured);
	static void askPassword(Window* window, const std::string& ssid);

	MenuComponent mMenu;
};

class GuiWifiSaved : public GuiComponent
{
public:
	GuiWifiSaved(Window* window, const std::vector<WifiNetwork>& networks);
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

private:
	void close();
	MenuComponent mMenu;
};

#endif // ES_APP_GUIS_GUI_WIFI_H
