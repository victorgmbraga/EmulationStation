#include "guis/GuiGeneralScreensaverOptions.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "Settings.h"

GuiGeneralScreensaverOptions::GuiGeneralScreensaverOptions(Window* window, const char* title) : GuiScreensaverOptions(window, title)
{
	// screensaver time
	auto screensaver_time = std::make_shared<SliderComponent>(mWindow, 0.f, 30.f, 1.f, "m");
	screensaver_time->setValue((float)(Settings::getInstance()->getInt("ScreenSaverTime") / Settings::ONE_MINUTE_IN_MS));
	addWithLabel("SCREENSAVER AFTER", screensaver_time);
	addSaveFunc([screensaver_time] {
		Settings::getInstance()->setInt("ScreenSaverTime", (int)Math::round(screensaver_time->getValue()) * Settings::ONE_MINUTE_IN_MS);
		PowerSaver::updateTimeouts();
	});

	// screensaver behavior
	auto screensaver_behavior = std::make_shared< OptionListComponent<std::string> >(mWindow, "SCREENSAVER BEHAVIOR", false);
	std::vector<std::string> screensavers;
	screensavers.push_back("dim");
	screensavers.push_back("black");
	bool isBlack = Settings::getInstance()->getString("ScreenSaverBehavior") == "black";
	for(auto it = screensavers.cbegin(); it != screensavers.cend(); it++)
		screensaver_behavior->add(*it, *it, (*it == "black") == isBlack);
	addWithLabel("SCREENSAVER BEHAVIOR", screensaver_behavior);
	addSaveFunc([screensaver_behavior] {
		Settings::getInstance()->setString("ScreenSaverBehavior", screensaver_behavior->getSelected());
		PowerSaver::updateTimeouts();
	});

	// system sleep time
	float stepw = 5.f;
	float max =  120.f;
	auto system_sleep_time = std::make_shared<SliderComponent>(mWindow, 0.f, max, stepw, "m");
	system_sleep_time->setValue((float)(Settings::getInstance()->getInt("SystemSleepTime") / Settings::ONE_MINUTE_IN_MS));
	addWithLabel("SYSTEM SLEEP AFTER", system_sleep_time);
	addSaveFunc([this, system_sleep_time, screensaver_time, max, stepw] {
		if (screensaver_time->getValue() > system_sleep_time->getValue() && system_sleep_time->getValue() > 0) {
			int steps = Math::min(1 + (int)(screensaver_time->getValue() / stepw), (int)(max/stepw));
			int adj_system_sleep_time = steps*stepw;
			system_sleep_time->setValue((float)adj_system_sleep_time);
			std::string msg = "";
			if (!Settings::getInstance()->getBool("SystemSleepTimeHintDisplayed")) {
				msg += "One time note: Enabling the system sleep time will trigger user-defined scripts.";
				msg += "\nPlease see Retropie/Emulationstation Wiki on events for details.";
				Settings::getInstance()->setBool("SystemSleepTimeHintDisplayed", true);
			}
			if (msg.length() > 0) {
				msg += "\n\n";
			}
			msg += "The system sleep delay is enabled, but is less than or equal to the screen saver start delay.";
			msg	+= "\n\nAdjusted system sleep time to " + std::to_string(adj_system_sleep_time) + " minutes.";
			mWindow->pushGui(new GuiMsgBox(mWindow, msg, "OK", [] { return; }));
		}
		Settings::getInstance()->setInt("SystemSleepTime", (int)Math::round(system_sleep_time->getValue()) * Settings::ONE_MINUTE_IN_MS);
	});
}

GuiGeneralScreensaverOptions::~GuiGeneralScreensaverOptions()
{
}
