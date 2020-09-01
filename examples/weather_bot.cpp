#include <cctype>
#include "telegrab.hpp"

#define OPEN_WEATHER_MAP_API_KEY "XXX"

void Telegrab::Instructions(incoming data)
{
	// First, let's check if the message contains any special commands
	for (const auto& entity:data.entities)
	{
		// On '/start' command
		if (entity == "/start")
		{
			// This variable contains all data that we want to send in response
			content message;

			// Adding some text. NOTE: If you want to send multiple files, they will all be sent in separate messages, and the text will be added to only one of them
			message.text = "Hi there! This bot can provide information about the current weather in your city. Type 'Help ℹ️' for more info.";

			// The Telegram keyboard consists of rows, which consist of buttons
			ReplyKeyboardRow row_1;

			// Creating button
			KeyboardButton btn;
			// Button text
			btn.text = "Help ℹ️";
			// Optional fields, 'false' by default, so you can delete these lines (for more info go to Telegram Bot API)
			btn.request_contact = false;
			btn.request_location = false;

			// Pushing button to the row
			row_1.push_back(btn);

			// Pushing row to the keyboard
			message.reply_keyboard.keyboard.push_back(row_1);
			// Optional fields, 'false' by default, so you can delete these lines (for more info go to Telegram Bot API)
			// resize_keyboard set to 'true' so the buttons look better
			message.reply_keyboard.resize_keyboard = true;
			message.reply_keyboard.one_time_keyboard = false;
			message.reply_keyboard.selective = false;

			// Sending our message to the same chat
			send(message, data.chat_id);

			return;
		}
	}

	// If incoming message contains text
	if (!data.text.empty())
	{
		// On button press
		std::transform(data.text.begin(), data.text.end(), data.text.begin(), [](unsigned char c){ return std::tolower(c); });
		if (data.text.find("help ℹ️") != std::string::npos)
		{
			content message;
			message.text = "To get information about the weather simply type the name of your sity (e.g. 'London' or 'london').";

			// Deleting custom keyboard
			ReplyKeyboardHide h;
			h.hide = true;
			// Optional field, 'false' by default
			h.selective = false;
			message.hide_reply_keyboard = h;

			send(message, data.chat_id);

			return;
		}

		// Receiving info about the weather

		httplib::Client cli("http://api.openweathermap.org");
		std::string key = OPEN_WEATHER_MAP_API_KEY;
		if (auto res = cli.Get(("/data/2.5/weather?units=metric&q=" + data.text + "&appid=" + key).c_str()))
		{
			if (res->status == 404)
			{
				content message;
				message.text = "Error: city not found.";
				ReplyKeyboardRow row_1;
				KeyboardButton btn;
				btn.text = "Help ℹ️";
				row_1.push_back(btn);
				message.reply_keyboard.keyboard.push_back(row_1);
				message.reply_keyboard.resize_keyboard = true;

				send(message, data.chat_id);

				return;
			}
			if (res->status == 429)
			{
				content message;
				message.text = "Error: sorry, too many requests, please try again in a few minutes.";
				
				send(message, data.chat_id);

				return;
			}
			if (res->status != 200)
			{
				content message;
				message.text = "Sorry, but we couldn't get information about the weather in that area.";
				send(message, data.chat_id);

				return;
			}
			nlohmann::json json = nlohmann::json::parse(res->body);

			content message;
			std::string foo = json["name"];
			message.text = "Weather in " + foo;
			foo = json["sys"]["country"];
			message.text += ", " + foo + ": ";
			for (const auto& item:json["weather"])
			{
				foo = item["description"];
				break;
			}
			message.text += foo + ".\n";
			message.text += "🌡 Temperature:\n";
			double temp = json["main"]["temp"];
			foo = std::to_string(temp);
			foo.erase(foo.find_last_not_of('0') + 1, std::string::npos);
			foo.erase(foo.find_last_not_of('.') + 1, std::string::npos);
			message.text += "\t\t\tCurrent: " + foo + "° C.\n";
			temp = json["main"]["temp_min"];
			foo = std::to_string(temp);
			foo.erase(foo.find_last_not_of('0') + 1, std::string::npos);
			foo.erase(foo.find_last_not_of('.') + 1, std::string::npos);
			message.text += "\t\t\tMin: " + foo + "° C.\n";
			temp = json["main"]["temp_max"];
			foo = std::to_string(temp);
			foo.erase(foo.find_last_not_of('0') + 1, std::string::npos);
			foo.erase(foo.find_last_not_of('.') + 1, std::string::npos);
			message.text += "\t\t\tMax: " + foo + "° C.\n";
			temp = json["wind"]["speed"];
			foo = std::to_string(temp);
			foo.erase(foo.find_last_not_of('0') + 1, std::string::npos);
			foo.erase(foo.find_last_not_of('.') + 1, std::string::npos);
			message.text += "💨 Wind speed: " + foo + " m/s.";

			ReplyKeyboardRow row_1;
			KeyboardButton btn;
			btn.text = "Help ℹ️";
			row_1.push_back(btn);
			message.reply_keyboard.keyboard.push_back(row_1);
			message.reply_keyboard.resize_keyboard = true;

			send(message, data.chat_id);
		}
		else
		{
			content message;
			message.text = "Sorry, but we couldn't get information about the weather in that area.";
			send(message, data.chat_id);
		}
	}
}

int main()
{
	Telegrab bot("123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11");
	bot.start();

	return 0;
}
