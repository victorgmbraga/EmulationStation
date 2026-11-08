#include "guis/GuiWifi.h"

#include "components/TextComponent.h"
#include "guis/GuiBusy.h"
#include "guis/GuiKeyboard.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "Window.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define WIFI_SCRIPT "/opt/btv9/wifi.sh"
#define WIFI_TEXT 0xBCBEBCFF

// Roda "sudo -n wifi.sh <args>" sem shell (o SSID vai como argumento, sem risco de aspas)
// e devolve a saída. A senha, quando há, vai pela entrada padrão.
static std::string runWifi(const std::vector<std::string>& args, const std::string& input = "")
{
	std::vector<std::string> all = { "/usr/bin/sudo", "-n", WIFI_SCRIPT };
	all.insert(all.end(), args.begin(), args.end());
	std::vector<char*> argv;
	for(auto& a : all)
		argv.push_back(const_cast<char*>(a.c_str()));
	argv.push_back(nullptr);

	int in[2], out[2];
	if(pipe(in) != 0)
		return "FALHA";
	if(pipe(out) != 0)
	{
		close(in[0]); close(in[1]);
		return "FALHA";
	}

	pid_t pid = fork();
	if(pid == 0)
	{
		dup2(in[0], 0);
		dup2(out[1], 1);
		int devnull = open("/dev/null", O_WRONLY);
		if(devnull >= 0)
			dup2(devnull, 2);
		close(in[0]); close(in[1]); close(out[0]); close(out[1]);
		execv(argv[0], argv.data());
		_exit(127);
	}
	close(in[0]);
	close(out[1]);
	if(pid < 0)
	{
		close(in[1]); close(out[0]);
		return "FALHA";
	}

	if(!input.empty())
	{
		// se o script já tiver saído, write() falha com EPIPE em vez de derrubar o ES
		sigset_t set, old;
		sigemptyset(&set);
		sigaddset(&set, SIGPIPE);
		pthread_sigmask(SIG_BLOCK, &set, &old);
		std::string line = input + "\n";
		ssize_t ignored = write(in[1], line.c_str(), line.size());
		(void)ignored;
		close(in[1]);
		struct timespec zero = { 0, 0 };
		sigtimedwait(&set, nullptr, &zero); // descarta um SIGPIPE pendente
		pthread_sigmask(SIG_SETMASK, &old, nullptr);
	}else{
		close(in[1]);
	}

	std::string result;
	char buf[4096];
	ssize_t n;
	while((n = read(out[0], buf, sizeof(buf))) > 0)
		result.append(buf, n);
	close(out[0]);
	int status;
	waitpid(pid, &status, 0);
	return result;
}

static std::vector< std::vector<std::string> > splitLines(const std::string& text)
{
	std::vector< std::vector<std::string> > lines;
	size_t start = 0;
	while(start < text.size())
	{
		size_t end = text.find('\n', start);
		if(end == std::string::npos)
			end = text.size();
		std::string line = text.substr(start, end - start);
		start = end + 1;
		if(line.empty())
			continue;
		std::vector<std::string> fields;
		size_t f = 0;
		while(true)
		{
			size_t t = line.find('\t', f);
			fields.push_back(line.substr(f, t == std::string::npos ? std::string::npos : t - f));
			if(t == std::string::npos)
				break;
			f = t + 1;
		}
		lines.push_back(fields);
	}
	return lines;
}

static std::string firstLine(const std::string& text)
{
	return text.substr(0, text.find('\n'));
}

// nomes muito longos são cortados para caber na linha
static std::string shortName(const std::string& ssid)
{
	return ssid.size() > 26 ? ssid.substr(0, 24) + ".." : ssid;
}

static bool isOpen(const std::string& security)
{
	return security.empty() || security == "--";
}

static HelpStyle wifiHelpStyle()
{
	HelpStyle style = HelpStyle();
	style.applyTheme(ViewController::get()->getState().getSystem()->getTheme(), "system");
	return style;
}

static void addNetworkRow(Window* window, MenuComponent& menu, const std::string& name, const std::string& detail,
	const std::function<void()>& onAccept)
{
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(window, name, Font::get(FONT_SIZE_MEDIUM), WIFI_TEXT), true);
	if(!detail.empty())
		row.addElement(std::make_shared<TextComponent>(window, "  " + detail, Font::get(FONT_SIZE_MEDIUM), WIFI_TEXT, ALIGN_RIGHT), false);
	if(onAccept)
		row.makeAcceptInputHandler(onAccept);
	menu.addRow(row);
}

// ---------------------------------------------------------------- lista de redes

void GuiWifi::open(Window* window)
{
	window->pushGui(new GuiBusy(window, "PROCURANDO REDES",
		[] { return runWifi({ "scan" }); },
		[window](const std::string& out) {
			std::vector<WifiNetwork> nets;
			for(auto& f : splitLines(out))
			{
				if(f.size() < 5)
					continue;
				nets.push_back(WifiNetwork { f[0] == "1", atoi(f[1].c_str()), f[2], f[3] == "1", f[4], "" });
			}
			window->pushGui(new GuiWifi(window, nets));
		}));
}

GuiWifi::GuiWifi(Window* window, const std::vector<WifiNetwork>& networks) : GuiComponent(window), mMenu(window, "WI-FI")
{
	if(networks.empty())
		addNetworkRow(window, mMenu, "NENHUMA REDE ENCONTRADA", "", nullptr);

	for(auto& net : networks)
	{
		std::string detail;
		if(net.active)
			detail = "CONECTADA";
		else
		{
			if(net.saved)
				detail += "SALVA  ";
			else if(isOpen(net.security))
				detail += "ABERTA  ";
			detail += std::to_string(net.signal) + "%";
		}
		addNetworkRow(window, mMenu, shortName(net.ssid), detail, [this, net] { select(net); });
	}

	mMenu.addButton("ATUALIZAR", "atualizar", [this] { Window* w = mWindow; delete this; open(w); });
	mMenu.addButton("REDES SALVAS", "redes salvas", [this] { Window* w = mWindow; delete this; openSaved(w); });
	mMenu.addButton("BACK", "back", [this] { delete this; });

	addChild(&mMenu);
	setSize(mMenu.getSize());
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiWifi::select(WifiNetwork net) // por valor: a lista é apagada logo abaixo
{
	Window* w = mWindow;
	if(net.active)
	{
		w->pushGui(new GuiMsgBox(w, "CONECTADO A " + net.ssid));
		return;
	}

	delete this;
	if(net.saved || isOpen(net.security))
		connect(w, net.ssid, "", !isOpen(net.security));
	else
		askPassword(w, net.ssid);
}

void GuiWifi::askPassword(Window* window, const std::string& ssid)
{
	window->pushGui(new GuiKeyboard(window, ssid, "",
		[window, ssid](const std::string& password) {
			// WPA: 8 a 63 caracteres (ou 64 em hexadecimal)
			if(password.size() < 8)
				window->pushGui(new GuiMsgBox(window, "A SENHA PRECISA TER PELO MENOS 8 CARACTERES", "OK",
					[window, ssid] { askPassword(window, ssid); }));
			else
				connect(window, ssid, password, true);
		}, 64,
		[window] { open(window); }));
}

void GuiWifi::connect(Window* window, const std::string& ssid, const std::string& password, bool secured)
{
	window->pushGui(new GuiBusy(window, "CONECTANDO A " + shortName(ssid),
		[ssid, password] { return firstLine(runWifi({ "connect", ssid }, password)); },
		[window, ssid, secured](const std::string& result) {
			auto reopen = [window] { open(window); };
			if(result == "OK")
				window->pushGui(new GuiMsgBox(window, "CONECTADO A " + ssid, "OK", reopen));
			else if(result == "SENHA" && secured)
				window->pushGui(new GuiMsgBox(window, "SENHA INCORRETA PARA " + ssid + ".\nTENTAR DE NOVO?",
					"YES", [window, ssid] { askPassword(window, ssid); }, "NO", reopen));
			else if(result == "NAOENCONTRADA")
				window->pushGui(new GuiMsgBox(window, "A REDE " + ssid + " NÃO FOI ENCONTRADA", "OK", reopen));
			else
				window->pushGui(new GuiMsgBox(window, "NÃO FOI POSSÍVEL CONECTAR A " + ssid, "OK", reopen));
		}));
}

bool GuiWifi::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if(config->isMappedTo("b", input) && input.value != 0)
	{
		delete this;
		return true;
	}
	return false;
}

std::vector<HelpPrompt> GuiWifi::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("a", "CONECTAR"));
	prompts.push_back(HelpPrompt("b", "back"));
	return prompts;
}

HelpStyle GuiWifi::getHelpStyle()
{
	return wifiHelpStyle();
}

// ---------------------------------------------------------------- redes salvas

void GuiWifi::openSaved(Window* window)
{
	window->pushGui(new GuiBusy(window, "CARREGANDO",
		[] { return runWifi({ "saved" }); },
		[window](const std::string& out) {
			std::vector<WifiNetwork> nets;
			for(auto& f : splitLines(out))
			{
				if(f.size() < 3)
					continue;
				nets.push_back(WifiNetwork { f[0] == "1", 0, "", true, f[2], f[1] });
			}
			window->pushGui(new GuiWifiSaved(window, nets));
		}));
}

GuiWifiSaved::GuiWifiSaved(Window* window, const std::vector<WifiNetwork>& networks) : GuiComponent(window), mMenu(window, "REDES SALVAS")
{
	if(networks.empty())
		addNetworkRow(window, mMenu, "NENHUMA REDE SALVA", "", nullptr);

	for(auto& net : networks)
	{
		const std::string ssid = net.ssid, uuid = net.uuid;
		const bool active = net.active;
		addNetworkRow(window, mMenu, shortName(ssid), active ? "CONECTADA" : "", [this, ssid, uuid, active] {
			std::string msg = "ESQUECER A REDE " + ssid + "?";
			if(active)
				msg += "\nO APARELHO VAI SE DESCONECTAR DELA.";
			mWindow->pushGui(new GuiMsgBox(mWindow, msg, "YES", [this, uuid] {
				Window* w = mWindow;
				delete this;
				w->pushGui(new GuiBusy(w, "ESQUECENDO",
					[uuid] { return runWifi({ "forget", uuid }); },
					[w](const std::string&) { GuiWifi::openSaved(w); }));
			}, "NO", nullptr));
		});
	}

	mMenu.addButton("BACK", "back", [this] { close(); });

	addChild(&mMenu);
	setSize(mMenu.getSize());
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiWifiSaved::close()
{
	// volta para a lista de redes (atualizada)
	Window* w = mWindow;
	delete this;
	GuiWifi::open(w);
}

bool GuiWifiSaved::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if(config->isMappedTo("b", input) && input.value != 0)
	{
		close();
		return true;
	}
	return false;
}

std::vector<HelpPrompt> GuiWifiSaved::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("a", "ESQUECER"));
	prompts.push_back(HelpPrompt("b", "back"));
	return prompts;
}

HelpStyle GuiWifiSaved::getHelpStyle()
{
	return wifiHelpStyle();
}
