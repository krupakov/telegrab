/*
╔════╗╔═══╗╔╗   ╔═══╗╔═══╗╔═══╗╔═══╗╔══╗
║╔╗╔╗║║╔══╝║║   ║╔══╝║╔═╗║║╔═╗║║╔═╗║║╔╗║  C++11 Telegram Bot API
╚╝║║╚╝║╚══╗║║   ║╚══╗║║ ╚╝║╚═╝║║║ ║║║╚╝╚╗ version 1.0
  ║║  ║╔══╝║║ ╔╗║╔══╝║║╔═╗║╔╗╔╝║╚═╝║║╔═╗║ https://github.com/krupakov/telegrab
  ║║  ║╚══╗║╚═╝║║╚══╗║╚╩═║║║║╚╗║╔═╗║║╚═╝║
  ╚╝  ╚═══╝╚═══╝╚═══╝╚═══╝╚╝╚═╝╚╝ ╚╝╚═══╝
MIT License

Copyright (c) 2020 Gleb Krupakov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <dirent.h>
#include "httplib.h"
#include "json.hpp"

struct KeyboardButton
{
	std::string text;
	bool request_contact;
	bool request_location;
};

using ReplyKeyboardRow = std::vector<KeyboardButton>;

struct ReplyKeyboardMarkup
{
	std::vector<ReplyKeyboardRow> keyboard;
	bool resize_keyboard;
	bool one_time_keyboard;
	bool selective;
};

struct ReplyKeyboardHide
{
	bool hide;
	bool selective;
};

struct incoming
{
	unsigned int chat_id;
	unsigned int message_id;
	std::string photo;
	std::string video;
	std::string document;
	std::string text;
	std::string audio;
	std::string sticker;
	std::string voice;
	std::string caption;
	std::vector<std::string> entities;
};

struct content
{
	std::string photo;
	std::string video;
	std::string document;
	std::string text;
	std::string audio;
	std::string sticker;
	ReplyKeyboardMarkup reply_keyboard;
	ReplyKeyboardHide hide_reply_keyboard;
};

class Telegrab
{
public:
	Telegrab(std::string token);
	void send(content message, unsigned int chat_id, unsigned int reply_to_message_id = 0);
	void forward(unsigned int message_id, unsigned int chat_id_from, unsigned int chat_id_to);
	void start();
	std::string download(std::string given);
private:
	unsigned int limit;
	unsigned int interval;
	unsigned int timeout;
	unsigned int retryTimeout;
	unsigned int last_update_id;
	unsigned int last_file_id;
	std::string bot_token;

	void Instructions(incoming data);
	void sendFile(std::string name, std::string text, unsigned int chat_id, unsigned char type, bool &caption, bool &rkeyboard, unsigned int reply_to_message_id, ReplyKeyboardMarkup reply_keyboard, ReplyKeyboardHide hide_reply_keyboard);
	bool waitForUpdates();

	bool fatalError;
};

Telegrab::Telegrab(std::string str):fatalError(false), last_update_id(0), last_file_id(0)
{
	try
	{
		/* Open or create config file */
		if (str.find(".json") != std::string::npos)
		{
			std::ifstream file(str);
			if (file.is_open())
			{
				nlohmann::json config = nlohmann::json::parse(file);
				limit = config["polling"]["limit"];
				interval = config["polling"]["interval"];
				timeout = config["polling"]["timeout"];
				retryTimeout = config["polling"]["retryTimeout"];
				bot_token = config["token"];

				file.close();
			}
			else
			{
				std::cerr << "\t| Error! Can't open " << str << "." << std::endl;
				throw 1;
			}
		}
		else
		{
			std::ifstream file(str + ".json");
			if (file.is_open())
			{
				nlohmann::json config = nlohmann::json::parse(file);
				limit = config["polling"]["limit"];
				interval = config["polling"]["interval"];
				timeout = config["polling"]["timeout"];
				retryTimeout = config["polling"]["retryTimeout"];
				file.close();
			}
			else
			{
				std::ofstream file(str + ".json", std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
				if (file.is_open())
				{
					nlohmann::json temp;
					temp["token"] = str;
					temp["polling"]["limit"] = 100; limit = 100;
					temp["polling"]["interval"] = 0; interval = 0;
					temp["polling"]["timeout"] = 10; timeout = 10;
					temp["polling"]["retryTimeout"] = 30; retryTimeout = 30;
					file << temp;
					file.close();
				}
				else
				{
					std::cerr << "\t| Error! Unable to create config file." << std::endl;
					throw 1;
				}
			}
			bot_token = str;
		}

		/* Create 'downloads' folder */
		if (chmod("downloads", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if (mkdir("downloads", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{
				std::cerr << "\t| Error! Unable to create 'downloads' folder." << std::endl;
				throw 1;
			}
		}

		/* Get ID of the last downloaded file (i.e. file_XXXX) */
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir("downloads")) != NULL)
		{
			std::string temp = "file_";
			while ((ent = readdir(dir)) != NULL)
			{
				if (ent->d_name[0] == temp[0] && ent->d_name[1] == temp[1] && ent->d_name[2] == temp[2] && ent->d_name[3] == temp[3] && ent->d_name[4] == temp[4])
				{
					last_file_id++;
				}
			}
			closedir(dir);
			last_file_id++;
		}
		else
		{
			std::cerr << "\t| Error! Unable to open 'downloads' folder." << std::endl;
			throw 1;
		}
	}
	catch (int)
	{
		fatalError = true;
	}
	catch (std::invalid_argument)
	{
		fatalError = true;
	}
}
bool Telegrab::waitForUpdates()
{
	httplib::Client cli("https://api.telegram.org");
	cli.enable_server_certificate_verification(false);
	httplib::Params params;

	params.emplace("limit", std::to_string(limit));
	if (timeout > 0)
	{
		params.emplace("timeout", std::to_string(timeout));
		cli.set_read_timeout(timeout + 1, 0);
	}
	if (last_update_id > 0)
	{
		params.emplace("offset", std::to_string(last_update_id + 1));
	}

	nlohmann::json file;
	if (auto res = cli.Post(("/bot" + bot_token + "/getUpdates").c_str(), params))
	{
		if (res->status != 200)
		{
			std::cerr << "\t| Error! Can't get updates. HTTP status code: " << res->status << "." << std::endl;
			return false;
		}
		file = nlohmann::json::parse(res->body);
	}
	else
	{
		std::cerr << "\t| Error! Can't get updates." << std::endl;
		std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
		return false;
	}

	if (file["ok"] && !file["result"].empty())
	{
		for (const auto& element:file["result"])
		{
			incoming message_data;
			std::string current = "message";
			if (element.count("edited_message") != 0)
			{
				current = "edited_message";
			}
			std::cout << "\tNew message from " << element[current]["from"]["first_name"];
			std::cout << "(" << element[current]["chat"]["id"] << ")." << std::endl;
			last_update_id = element["update_id"];
			message_data.chat_id = element[current]["chat"]["id"];
			message_data.message_id = element[current]["message_id"];
			if (element[current].count("photo") != 0)
			{
				for (const auto& image:element[current]["photo"])
				{
					if (image.count("file_size") != 0)
					{
						if (image["file_size"] > 20900000) break;
					}
					message_data.photo = image["file_id"];
				}
			}
			else
			{
				message_data.photo = "";
			}
			if (element[current].count("video") != 0)
				message_data.video = element[current]["video"]["file_id"];
			else message_data.video = "";
			if (element[current].count("document") != 0)
				message_data.document = element[current]["document"]["file_id"];
			else message_data.document = "";
			if (element[current].count("text") != 0)
				message_data.text = element[current]["text"];
			else message_data.text = "";
			if (element[current].count("audio") != 0)
				message_data.audio = element[current]["audio"]["file_id"];
			else message_data.audio = "";
			if (element[current].count("sticker") != 0)
				message_data.sticker = element[current]["sticker"]["file_id"];
			else message_data.sticker = "";
			if (element[current].count("voice") != 0)
				message_data.voice = element[current]["voice"]["file_id"];
			else message_data.voice = "";
			if (element[current].count("caption") != 0)
				message_data.caption = element[current]["caption"];
			else message_data.caption = "";
			if (element[current].count("entities") != 0)
			{
				unsigned short int k = 0;
				for (const auto& entity:element[current]["entities"])
				{
					message_data.entities.push_back("");
					unsigned short int t1 = entity["offset"];
					unsigned short int t2 = entity["length"];
					for (unsigned short int i = t1; i < (t1 + t2); i++)
					{
						message_data.entities[k] += message_data.text[i];
					}
					k++;
				}
			}

			Instructions(message_data);
		}
	}
	return true;
}
void Telegrab::send(content message, unsigned int chat_id, unsigned int reply_to_message_id)
{
	/* Since we don't know for what file in the message the text refers to,
	we simply create a boolean 'caption' to let the program know, if the text has already been sent */
	/* Same goes for rkeyboard */
	bool caption = false, rkeyboard = false;
	if (!message.photo.empty())
		sendFile(message.photo, message.text, chat_id, 1, caption, rkeyboard, reply_to_message_id, message.reply_keyboard, message.hide_reply_keyboard);
	if (!message.video.empty())
		sendFile(message.video, message.text, chat_id, 2, caption, rkeyboard, reply_to_message_id, message.reply_keyboard, message.hide_reply_keyboard);
	if (!message.document.empty())
		sendFile(message.document, message.text, chat_id, 3, caption, rkeyboard, reply_to_message_id, message.reply_keyboard, message.hide_reply_keyboard);
	if (!message.audio.empty())
		sendFile(message.audio, message.text, chat_id, 4, caption, rkeyboard, reply_to_message_id, message.reply_keyboard, message.hide_reply_keyboard);
	if (!message.sticker.empty())
		sendFile(message.sticker, message.text, chat_id, 5, caption, rkeyboard, reply_to_message_id, message.reply_keyboard, message.hide_reply_keyboard);
	if (!message.text.empty() && !caption)
	{
		std::cout << "\tSending a message to " << chat_id << "..." << std::endl;
		httplib::Client cli("https://api.telegram.org");
		cli.enable_server_certificate_verification(false);
		httplib::Params params;

		params.emplace("chat_id", std::to_string(chat_id));
		params.emplace("text", message.text);
		if (reply_to_message_id != 0)
		{
			params.emplace("reply_to_message_id", std::to_string(reply_to_message_id));
		}
		if (!message.reply_keyboard.keyboard.empty() && !rkeyboard)
		{
			nlohmann::json keyboard;
			unsigned int i = 0, j;
			for (const auto& element:message.reply_keyboard.keyboard)
			{
				j = 0;
				for (const auto& e:element)
				{
					if (!e.text.empty())
					{
						keyboard["keyboard"][i][j]["text"] = e.text;
					}
					if (e.request_contact == true)
					{
						keyboard["keyboard"][i][j]["request_contact"] = true;
					}
					if (e.request_location == true)
					{
						keyboard["keyboard"][i][j]["request_location"] = true;
					}
					j++;
				}
				i++;
			}
			if (message.reply_keyboard.resize_keyboard == true)
			{
				keyboard["resize_keyboard"] = true;
			}
			if (message.reply_keyboard.one_time_keyboard == true)
			{
				keyboard["one_time_keyboard"] = true;
			}
			if (message.reply_keyboard.selective == true)
			{
				keyboard["selective"] = true;
			}

			std::string serialized = keyboard.dump();
			params.emplace("reply_markup", serialized);
			
			rkeyboard = true;
		}
		if (message.hide_reply_keyboard.hide == true && !rkeyboard)
		{
			nlohmann::json json;
			json["hide_keyboard"] = true;
			if (message.hide_reply_keyboard.selective == true)
			{
				json["selective"] = true;
			}

			std::string serialized = json.dump();
			params.emplace("reply_markup", serialized);
		}

		if (auto res = cli.Post(("/bot" + bot_token + "/sendMessage").c_str(), params))
		{
			if (res->status != 200)
			{
				std::cerr << "\t| Error! Can't send a text message to " << chat_id  << ". HTTP status code: " << res->status << "." << std::endl;
			}
			else
			{
				std::cout << "\tSuccessfully sent." << std::endl;
			}
		}
		else
		{
			std::cerr << "\t| Error! Can't send a text message to " << chat_id  << "." << std::endl;
			std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
		}
	}
}
void Telegrab::forward(unsigned int message_id, unsigned int chat_id_from, unsigned int chat_id_to)
{
	std::cout << "\tForwarding the message " << message_id << " to " << chat_id_to << "..." << std::endl;
	httplib::Client cli("https://api.telegram.org");
	cli.enable_server_certificate_verification(false);
	httplib::Params params;

	params.emplace("chat_id", std::to_string(chat_id_to));
	params.emplace("from_chat_id", std::to_string(chat_id_from));
	params.emplace("message_id", std::to_string(message_id));

	if (auto res = cli.Post(("/bot" + bot_token + "/forwardMessage").c_str(), params))
	{
		if (res->status != 200)
		{
			std::cerr << "\t| Error! Can't forward a message " << message_id << " to " << chat_id_to  << ". HTTP status code: " << res->status << "." << std::endl;
		}
		else
		{
			std::cout << "\tSuccessfully sent." << std::endl;
		}
	}
	else
	{
		std::cerr << "\t| Error! Can't forward a message " << message_id << " to " << chat_id_to  << "." << std::endl;
		std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
	}
}
void Telegrab::sendFile(std::string name, std::string text, unsigned int chat_id, unsigned char type, bool &caption, bool &rkeyboard, unsigned int reply_to_message_id, ReplyKeyboardMarkup reply_keyboard, ReplyKeyboardHide hide_reply_keyboard)
{
	std::cout << "\tSending a file to " << chat_id << "..." << std::endl;
	httplib::Client cli("https://api.telegram.org");
	cli.enable_server_certificate_verification(false);
	std::string url = "/bot" + bot_token;
	std::ifstream file(name, std::ios::in | std::ios::binary);
	if (file.is_open())
	{
		std::string mimeType, contentType;
		std::stringstream filedata;
		filedata << file.rdbuf();
		file.close();

		/* Getting MIME type */
		system(("file --mime-type -b " + name + " > mime.log").c_str());
		std::ifstream mime("mime.log");
		getline(mime, mimeType, '\0');
		mime.close();

		switch (type)
		{
			case 1:
				url += "/sendPhoto";
				contentType = "photo";
				break;
			case 2:
				url += "/sendVideo";
				contentType = "video";
				break;
			case 3:
				url += "/sendDocument";
				contentType = "document";
				break;
			case 4:
				url += "/sendAudio";
				contentType = "audio";
				break;
			case 5:
				url += "/sendSticker";
				contentType = "sticker";
				break;
		}

		httplib::MultipartFormDataItems items = {};
		items.push_back({contentType, filedata.str(), name, mimeType});
		items.push_back({"chat_id", std::to_string(chat_id), "", ""});
		if (!text.empty() && !caption && type != 5)
		{
			items.push_back({"caption", text, "", ""});
			caption = true;
		}
		if (reply_to_message_id != 0)
		{
			items.push_back({"reply_to_message_id", std::to_string(reply_to_message_id), "", ""});
		}
		if (!reply_keyboard.keyboard.empty() && !rkeyboard)
		{
			nlohmann::json keyboard;
			unsigned int i = 0, j;
			for (const auto& element:reply_keyboard.keyboard)
			{
				j = 0;
				for (const auto& e:element)
				{
					if (!e.text.empty())
					{
						keyboard["keyboard"][i][j]["text"] = e.text;
					}
					if (e.request_contact == true)
					{
						keyboard["keyboard"][i][j]["request_contact"] = true;
					}
					if (e.request_location == true)
					{
						keyboard["keyboard"][i][j]["request_location"] = true;
					}
					j++;
				}
				i++;
			}
			if (reply_keyboard.resize_keyboard == true)
			{
				keyboard["resize_keyboard"] = true;
			}
			if (reply_keyboard.one_time_keyboard == true)
			{
				keyboard["one_time_keyboard"] = true;
			}
			if (reply_keyboard.selective == true)
			{
				keyboard["selective"] = true;
			}

			std::string serialized = keyboard.dump();
			items.push_back({"reply_markup", serialized, "", ""});

			rkeyboard = true;
		}
		if (hide_reply_keyboard.hide == true && !rkeyboard)
		{
			nlohmann::json json;
			json["hide_keyboard"] = true;
			if (hide_reply_keyboard.selective == true)
			{
				json["selective"] = true;
			}

			std::string serialized = json.dump();
			items.push_back({"reply_markup", serialized, "", ""});
		}

		if (auto res = cli.Post(url.c_str(), items))
		{
			if (res->status != 200)
			{
				std::cerr << "\t| Error! Can't send a file to " << chat_id  << ". Perhaps the file is too big. HTTP status code: " << res->status << "." << std::endl;
			}
			else
			{
				std::cout << "\tSuccessfully sent." << std::endl;
			}
		}
		else
		{
			std::cerr << "\t| Error! Can't send a file to " << chat_id  << "." << std::endl;
			std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
		}
	}
	else
	{
		httplib::Params params;
		params.emplace("chat_id", std::to_string(chat_id));

		switch (type)
		{
			case 1:
				url += "/sendPhoto";
				params.emplace("photo", name);
				break;
			case 2:
				url += "/sendVideo";
				params.emplace("video", name);
				break;
			case 3:
				url += "/sendDocument";
				params.emplace("document", name);
				break;
			case 4:
				url += "/sendAudio";
				params.emplace("audio", name);
				break;
			case 5:
				url += "/sendSticker";
				params.emplace("sticker", name);
				break;
		}
		if (!text.empty() && !caption && type != 5)
		{
			params.emplace("caption", text);
			caption = true;
		}
		if (reply_to_message_id != 0)
		{
			params.emplace("reply_to_message_id", std::to_string(reply_to_message_id));
		}
		if (!reply_keyboard.keyboard.empty() && !rkeyboard)
		{
			nlohmann::json keyboard;
			unsigned int i = 0, j;
			for (const auto& element:reply_keyboard.keyboard)
			{
				j = 0;
				for (const auto& e:element)
				{
					if (!e.text.empty())
					{
						keyboard["keyboard"][i][j]["text"] = e.text;
					}
					if (e.request_contact == true)
					{
						keyboard["keyboard"][i][j]["request_contact"] = true;
					}
					if (e.request_location == true)
					{
						keyboard["keyboard"][i][j]["request_location"] = true;
					}
					j++;
				}
				i++;
			}
			if (reply_keyboard.resize_keyboard == true)
			{
				keyboard["resize_keyboard"] = true;
			}
			if (reply_keyboard.one_time_keyboard == true)
			{
				keyboard["one_time_keyboard"] = true;
			}
			if (reply_keyboard.selective == true)
			{
				keyboard["selective"] = true;
			}

			std::string serialized = keyboard.dump();
			params.emplace("reply_markup", serialized);

			rkeyboard = true;
		}
		if (hide_reply_keyboard.hide == true && !rkeyboard)
		{
			nlohmann::json json;
			json["hide_keyboard"] = true;
			if (hide_reply_keyboard.selective == true)
			{
				json["selective"] = true;
			}

			std::string serialized = json.dump();
			params.emplace("reply_markup", serialized);
		}

		if (auto res = cli.Post(url.c_str(), params))
		{
			if (res->status != 200)
			{
				std::cerr << "\t| Error! Can't send " << name << " to " << chat_id  << ". HTTP status code: " << res->status << "." << std::endl;
			}
			else
			{
				std::cout << "\tSuccessfully sent." << std::endl;
			}
		}
		else
		{
			std::cerr << "\t| Error! Can't send " << name << " to " << chat_id  << "." << std::endl;
			std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
		}
	}
}
std::string Telegrab::download(std::string given)
{
	std::cout << "\tTrying to download " << given << "..." << std::endl;
	if (given.empty())
	{
		std::cerr << "\t| Error! Given string is empty." << std::endl;
		return "";
	}
	/* Check if the given string is a link (file_id doesn't contain dots) */
	if (given.find(".") != std::string::npos)
	{
		std::string file_path = "downloads/file_" + std::to_string(last_file_id);
		last_file_id++;

		/* Getting domain separated */
		unsigned int i = 0;
		std::string domain = "", url = "";
		if (given.find("http://") != std::string::npos)
		{
			for (unsigned int j = 7; j < given.size(); j++)
			{
				if (given[j] == '/')
				{
					i = j;
					break;
				}
				domain += given[j];
			}
		}
		if (given.find("https://") != std::string::npos)
		{
			for (unsigned int j = 8; j < given.size(); j++)
			{
				if (given[j] == '/')
				{
					i = j;
					break;
				}
				domain += given[j];
			}
		}
		if (domain != "")
		{
			for (i; i < given.size(); i++)
			{
				url += given[i];
			}
		}
		else
		{
			for (i; i < given.size(); i++)
			{
				if (given[i] == '/')
				{
					for (unsigned int j = i; j < given.size(); j++)
					{
						url += given[i];
					}
					break;
				}
				domain += given[i];
			}
		}

		/* File download */
		httplib::Client cli(domain.c_str());
		cli.enable_server_certificate_verification(false);
		std::string body;
		auto res = cli.Get(url.c_str(), [&body](const char *data, size_t data_length)
		{
			body.append(data, data_length);
			return true;
		});
		if (res)
		{
			if (res->status != 200)
			{
				std::cerr << "\t| Error! Can't download " << given << ". HTTP status code: " << res->status << "." << std::endl;
				return "";
			}
		}
		else
		{
			std::cerr << "\t| Error! Can't download " << given << "." << std::endl;
			std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
			return "";
		}

		std::ofstream file(file_path, std::ios_base::out | std::ios_base::binary);
		if (file.is_open())
		{
			file << body;
			file.close();
			std::cout << "\tSuccessfully downloaded." << std::endl;
			return file_path;
		}
	}
	else
	{
		httplib::Client cli("https://api.telegram.org");
		cli.enable_server_certificate_verification(false);
		httplib::Params params;
		params.emplace("file_id", given);

		std::string buffer;
		if (auto res = cli.Post(("/bot" + bot_token + "/getFile").c_str(), params))
		{
			if (res->status != 200)
			{
				std::cerr << "\t| Error! Can't get a file_path to download the file. Perhaps the file is too big. HTTP status code: " << res->status << "." << std::endl;
				return "";
			}
			else
			{
				buffer = res->body;
			}
		}
		else
		{
			std::cerr << "\t| Error! Can't get a file_path to download the file." << std::endl;
			std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
			return "";
		}
		nlohmann::json result = nlohmann::json::parse(buffer);

		if (result["ok"])
		{
			std::string file_path = result["result"]["file_path"], newdir = "downloads/";
			for (unsigned int i = 0; i < file_path.size(); i++)
			{
				if (file_path[i] != '/')
				{
					newdir += file_path[i];
				}
				else break;
			}
			std::string path = "downloads/" + file_path;
			short err = chmod(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (err == -1)
			{
				err = mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			}
			if (err != -1)
			{
				std::string body;
				auto res = cli.Get(("/file/bot" + bot_token + "/" + file_path).c_str(), [&body](const char *data, size_t data_length)
				{
					body.append(data, data_length);
					return true;
				});
				if (res)
				{
					if (res->status != 200)
					{
						std::cerr << "\t| Error! Can't download " << given << ". Perhaps the file is too big. HTTP status code: " << res->status << "." << std::endl;
						return "";
					}
				}
				else
				{
					std::cerr << "\t| Error! Can't download " << given << ". Perhaps the file is too big." << std::endl;
					std::cerr << "\t| HTTPLIB Error code: " << res.error() << std::endl;
					return "";
				}

				std::ofstream file(path, std::ios_base::out | std::ios_base::binary);
				if (file.is_open())
				{
					file << body;
					file.close();
					std::cout << "\tSuccessfully downloaded." << std::endl;
					return path;
				}
				else
				{
					std::cerr << "\t| Error! Can't download " << given << ". Error creating new file." << std::endl;
				}
			}
			else std::cerr << "\t| Error! Can't create a folder for the file." << std::endl;
		}
		else std::cerr << "\t| Error! Can't download " << given << "." << std::endl;
	}
	return "";
}
void Telegrab::start()
{
	if (!fatalError)
	{
		std::cout << "\tChecking for updates..." << std::endl;
		while (true)
		{
			if (!waitForUpdates())
			{
				std::cerr << "\t| Error! Failed to connect. Reconnecting in " << retryTimeout << " seconds..." << std::endl;
				if (retryTimeout > 0)
				{
					std::this_thread::sleep_for(std::chrono::seconds(retryTimeout));
					std::cout << "\tChecking for updates..." << std::endl;
				}
			}
			if (interval > 0)
			{
				std::this_thread::sleep_for(std::chrono::seconds(interval));
				std::cout << "\tChecking for updates..." << std::endl;
			}
		}
	}
}
