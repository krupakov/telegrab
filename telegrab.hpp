/*
╔════╗╔═══╗╔╗   ╔═══╗╔═══╗╔═══╗╔═══╗╔══╗
║╔╗╔╗║║╔══╝║║   ║╔══╝║╔═╗║║╔═╗║║╔═╗║║╔╗║  C++11 Telegram Bot API
╚╝║║╚╝║╚══╗║║   ║╚══╗║║ ╚╝║╚═╝║║║ ║║║╚╝╚╗ version 0.9.3
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

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <curl/curl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "json.hpp"

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
};

static size_t curlWriter(char *data, size_t size, size_t nmemb, std::string *buffer)
{
	size_t result = size * nmemb;
	buffer->append(data, result);
	return result;
}

static size_t curlFileWriter(char *data, size_t size, size_t nmemb, std::ofstream *file)
{
	size_t result = size * nmemb;
	file->write(data, result);
	return result;
}

class Telegrab
{
public:
	Telegrab(std::string token);
	~Telegrab();
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
	std::string basic_url;
	std::string download_url;
	std::string config_filename;

	void Instructions(incoming data);
	bool waitForUpdates();
	bool sendFile(std::string name, std::string text, unsigned int chat_id, unsigned char type, bool caption, unsigned int reply_to_message_id);

	CURL* CurlInit();

	bool fatalError;

	std::mutex mtx;
};

Telegrab::Telegrab(std::string str):fatalError(false), last_file_id(1)
{
	try
	{
		/* Open or create config file */
		if (str.find(".json") != std::string::npos)
		{
			config_filename = str;
			std::ifstream file(str);
			if (file.is_open())
			{
				nlohmann::json config = nlohmann::json::parse(file);
				last_update_id = config["last_update_id"];
				limit = config["polling"]["limit"];
				interval = config["polling"]["interval"];
				timeout = config["polling"]["timeout"];
				retryTimeout = config["polling"]["retryTimeout"];

				std::string temp = config["token"];
				basic_url = "https://api.telegram.org/bot" + temp;
				download_url = "https://api.telegram.org/file/bot" + temp + "/";
				file.close();
			}
			else
				{ std::cerr << "\t| Error! Can't open " << str << "." << std::endl; throw 1; }
		}
		else
		{
			config_filename = str + ".json";
			std::ifstream file(config_filename);
			if (file.is_open())
			{
				nlohmann::json config = nlohmann::json::parse(file);
				last_update_id = config["last_update_id"];
				limit = config["polling"]["limit"];
				interval = config["polling"]["interval"];
				timeout = config["polling"]["timeout"];
				retryTimeout = config["polling"]["retryTimeout"];
				file.close();
			}
			else
			{
				std::ofstream file(config_filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
				if (file.is_open())
				{
					nlohmann::json temp;
					temp["token"] = str;
					temp["last_update_id"] = 0; last_update_id = 0;
					temp["polling"]["limit"] = 100; limit = 100;
					temp["polling"]["interval"] = 0; interval = 0;
					temp["polling"]["timeout"] = 30; timeout = 30;
					temp["polling"]["retryTimeout"] = 30; retryTimeout = 30;
					file << temp;
					file.close();
				}
				else
					{ std::cerr << "\t| Error! Unable to create config file." << std::endl; throw 1; }
			}
			basic_url = "https://api.telegram.org/bot" + str;
			download_url = "https://api.telegram.org/file/bot" + str + "/";
		}

		/* Create 'downloads' folder */
		if (chmod("downloads", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			if (mkdir("downloads", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
				{ std::cerr << "\t| Error! Unable to create 'downloads' folder." << std::endl; throw 1; }

		/* Get ID of the last downloaded file (i.e. file_XXXX) */
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir("downloads")) != NULL)
		{
			std::string temp = "file_", name = "";
			while ((ent = readdir(dir)) != NULL)
				if (ent->d_name[0] == temp[0] && ent->d_name[1] == temp[1] && ent->d_name[2] == temp[2] && ent->d_name[3] == temp[3] && ent->d_name[4] == temp[4])
					{ name.assign(ent->d_name, strlen(ent->d_name)); break; }
			closedir(dir);
			if (name.size() > 5)
			{
				temp = "";
				for (unsigned short i = 5; i < name.size(); i++)
					temp += name[i];
				last_file_id = std::stoi(temp);
				last_file_id++;
			}
		}
		else
			{ std::cerr << "\t| Error! Unable to open 'downloads' folder." << std::endl; throw 1; }

		curl_global_init(CURL_GLOBAL_DEFAULT);
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
Telegrab::~Telegrab()
{
	curl_global_cleanup();
}
CURL* Telegrab::CurlInit()
{
	CURL *curl = nullptr;
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriter);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	return curl;
}
bool Telegrab::waitForUpdates()
{
	CURL *curl = CurlInit();
	if (!curl)
		{ std::cerr << "\t| Error! Can't get updates. cURL is not working properly." << std::endl; return false; }
	std::string buffer;
	std::string url = basic_url + "/getUpdates";
	std::string post_url = "limit=" + std::to_string(limit);
	if (timeout > 0)
		post_url += "&timeout=" + std::to_string(timeout);
	if (last_update_id > 0)
		post_url += "&offset=" + std::to_string(last_update_id + 1);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK)
		{ std::cerr << "\t| Error! Can't get updates." << std::endl; return false; }
	nlohmann::json file = nlohmann::json::parse(buffer);
	if (file["ok"] && !file["result"].empty())
	{
		for (const auto& element:file["result"])
		{
			incoming message_data;
			std::string current = "message";
			if (element.count("edited_message") != 0)
				current = "edited_message";
			std::cout << "\tNew message from " << element[current]["from"]["first_name"];
			std::cout << "(" << element[current]["chat"]["id"] << ")." << std::endl;
			last_update_id = element["update_id"];
			message_data.chat_id = element[current]["chat"]["id"];
			message_data.message_id = element[current]["message_id"];
			if (element[current].count("photo") != 0)
				for (const auto& image:element[current]["photo"])
				{
					if (image.count("file_size") != 0)
						if (image["file_size"] > 20900000)
							break;
					message_data.photo = image["file_id"];
				}
			else message_data.photo = "";
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
						message_data.entities[k] += message_data.text[i];
					k++;
				}
			}
			std::thread msg(&Telegrab::Instructions, this, message_data);
			msg.detach();
		}
		std::ifstream file(config_filename);
		if (file.is_open())
		{
			nlohmann::json config = nlohmann::json::parse(file);
			config["last_update_id"] = last_update_id;
			file.close();
			std::ofstream newfile(config_filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			if (newfile.is_open())
			{
				newfile << config;
				newfile.close();
			}
		}
	}
	return true;
}
void Telegrab::send(content message, unsigned int chat_id, unsigned int reply_to_message_id)
{
	CURL *curl = CurlInit();
	if (!curl)
		{ std::cerr << "\t| Error! Can't send a text message to " << chat_id  << ". cURL is not working properly." << std::endl; return; }
	std::cout << "\tSending a message to " << chat_id << "..." << std::endl;
	/* Since we don't know for what file in the message the text refers to,
	we simply create a boolean 'caption', which meant to let the program know, if the text has already been sent */
	bool caption = false;
	if (!message.photo.empty())
		caption = sendFile(message.photo, message.text, chat_id, 1, caption, reply_to_message_id);
	if (!message.video.empty())
		caption = sendFile(message.video, message.text, chat_id, 2, caption, reply_to_message_id);
	if (!message.document.empty())
		caption = sendFile(message.document, message.text, chat_id, 3, caption, reply_to_message_id);
	if (!message.audio.empty())
		caption = sendFile(message.audio, message.text, chat_id, 4, caption, reply_to_message_id);
	if (!message.sticker.empty())
		sendFile(message.sticker, message.text, chat_id, 5, caption, reply_to_message_id);
	if (!message.text.empty() && !caption)
	{
		std::string buffer;
		std::string url = basic_url + "/sendMessage";
		std::string post_url = "chat_id=" + std::to_string(chat_id) + "&text=" + message.text;
		if (reply_to_message_id != 0)
			post_url += "&reply_to_message_id=" + std::to_string(reply_to_message_id);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res != CURLE_OK)
			std::cerr << "\t| Error! Can't send a text message to " << chat_id  << "." << std::endl;
	}
}
void Telegrab::forward(unsigned int message_id, unsigned int chat_id_from, unsigned int chat_id_to)
{
	CURL *curl = CurlInit();
	if (!curl)
		{ std::cerr << "\t| Error! Can't forward a message " << message_id << " to " << chat_id_to  << ". cURL is not working properly." << std::endl; return; }
	std::cout << "\tForwarding the message " << message_id << " to " << chat_id_to << "..." << std::endl;
	std::string buffer;
	std::string url = basic_url + "/forwardMessage";
	std::string post_url = "chat_id=" + std::to_string(chat_id_to) + "&from_chat_id=" + std::to_string(chat_id_from) + "&message_id=" + std::to_string(message_id);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
		if (res != CURLE_OK)
			std::cerr << "\t| Error! Can't forward a message " << message_id << " to " << chat_id_to  << "." << std::endl;
}
bool Telegrab::sendFile(std::string name, std::string text, unsigned int chat_id, unsigned char type, bool caption, unsigned int reply_to_message_id)
{
	bool cap = caption;
	std::string buffer;
	std::string url;
	std::ifstream file(name);
	if (file.is_open())
	{
		file.close();
		CURL *curl_multipart = CurlInit();
		curl_mime *form = nullptr;
		curl_mimepart *field = nullptr;
		curl_easy_setopt(curl_multipart, CURLOPT_WRITEDATA, &buffer);
		curl_easy_setopt(curl_multipart, CURLOPT_POST, 0);
		if (!curl_multipart)
			std::cerr << "\t| Error! Can't send a file to " << chat_id  << ". cURL is not working properly." << std::endl;
		else
		{
			form = curl_mime_init(curl_multipart);
			field = curl_mime_addpart(form);
			switch (type)
			{
				case 1:
					curl_mime_name(field, "photo");
					url = basic_url + "/sendPhoto";
					break;
				case 2:
					curl_mime_name(field, "video");
					url = basic_url + "/sendVideo";
					break;
				case 3:
					curl_mime_name(field, "document");
					url = basic_url + "/sendDocument";
					break;
				case 4:
					curl_mime_name(field, "audio");
					url = basic_url + "/sendAudio";
					break;
				case 5:
					curl_mime_name(field, "sticker");
					url = basic_url + "/sendSticker";
					break;
			}
			curl_mime_filedata(field, name.c_str());
			field = curl_mime_addpart(form);
			curl_mime_name(field, "chat_id");
			curl_mime_data(field, std::to_string(chat_id).c_str(), CURL_ZERO_TERMINATED);
			if (!text.empty() && !caption && type != 5)
			{
				field = curl_mime_addpart(form);
				curl_mime_name(field, "caption");
				curl_mime_data(field, text.c_str(), CURL_ZERO_TERMINATED);
				cap = true;
			}
			if (reply_to_message_id != 0)
			{
				field = curl_mime_addpart(form);
				curl_mime_name(field, "reply_to_message_id");
				curl_mime_data(field, std::to_string(reply_to_message_id).c_str(), CURL_ZERO_TERMINATED);
			}
			curl_easy_setopt(curl_multipart, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl_multipart, CURLOPT_MIMEPOST, form);
			CURLcode res = curl_easy_perform(curl_multipart);
			if (res != CURLE_OK)
				std::cerr << "\t| Error! Can't send a file to " << chat_id  << ". Perhaps the file is too large." << std::endl;

			curl_easy_cleanup(curl_multipart);
			curl_mime_free(form);
		}
	}
	else
	{
		CURL *curl = CurlInit();
		if (!curl)
			{ std::cerr << "\t| Error! Can't send a file to " << chat_id  << ". cURL is not working properly." << std::endl; return cap; }
		std::string post_url = "chat_id=" + std::to_string(chat_id);
		switch (type)
		{
			case 1:
				url = basic_url + "/sendPhoto";
				post_url += "&photo=" + name;
				break;
			case 2:
				url = basic_url + "/sendVideo";
				post_url += "&video=" + name;
				break;
			case 3:
				url = basic_url + "/sendDocument";
				post_url += "&document=" + name;
				break;
			case 4:
				url = basic_url + "/sendAudio";
				post_url += "&audio=" + name;
				break;
			case 5:
				url = basic_url + "/sendSticker";
				post_url += "&sticker=" + name;
				break;
		}
		if (!text.empty() && !caption && type != 5)
		{
			post_url += "&caption=" + text;
			cap = true;
		}
		if (reply_to_message_id != 0)
			post_url += "&reply_to_message_id=" + std::to_string(reply_to_message_id);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res != CURLE_OK)
			std::cerr << "\t| Error! Can't send " << name << " to " << chat_id  << "." << std::endl;
	}
	return cap;
}
std::string Telegrab::download(std::string given)
{
	std::cout << "\tTrying to download " << given << "..." << std::endl;
	CURL *curl = CurlInit();
	if (!curl)
		{ std::cerr << "\t| Error! Can't download " << given << ". cURL is not working properly." << std::endl; return ""; }
	if (given.empty())
		{ std::cerr << "\t| Error! Given string is empty." << std::endl; curl_easy_cleanup(curl); return ""; }
	/* Check if the given string is a link (file_id doesn't contain dots) */
	if (given.find(".") != std::string::npos)
	{
		mtx.lock();
		std::string file_path = "downloads/file_" + std::to_string(last_file_id);
		last_file_id++;
		mtx.unlock();

		curl_easy_setopt(curl, CURLOPT_URL, given.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 0);
		std::ofstream file(file_path, std::ios_base::out | std::ios_base::binary);
		if (file.is_open())
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlFileWriter);
			CURLcode res = curl_easy_perform(curl);
			curl_easy_cleanup(curl);
			file.close();
			if (res != CURLE_OK)
				{ std::cerr << "\t| Error! Can't download " << given << "." << std::endl; return ""; }
			std::cout << "\tSuccessfully downloaded." << std::endl;
			return file_path;
		}
	}
	else
	{
		std::string buffer;
		std::string url = basic_url + "/getFile", post_url = "file_id=" + given;
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res != CURLE_OK)
			{ std::cerr << "\t| Error! Can't get a file_path to download the file." << std::endl; return ""; }
		nlohmann::json result = nlohmann::json::parse(buffer);
		if (result["ok"])
		{
			std::string file_path, newdir = "downloads/";
			file_path = result["result"]["file_path"];
			url = download_url + file_path;
			for (unsigned int i = 0; i < file_path.size(); i++)
				if (file_path[i] != '/')
					newdir += file_path[i];
				else break;
			file_path = "downloads/" + file_path;
			short err = chmod(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (err == -1)
				err = mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (err != -1)
			{
				curl = CurlInit();
				if (!curl)
					{ std::cerr << "\t| Error! Can't download " << given << ". cURL is not working properly." << std::endl; return ""; }
				std::ofstream file(file_path, std::ios_base::out | std::ios_base::binary);
				if (file.is_open())
				{
					curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlFileWriter);
					curl_easy_setopt(curl, CURLOPT_POST, 0);
					res = curl_easy_perform(curl);
					curl_easy_cleanup(curl);
					file.close();
					if (res != CURLE_OK)
						{ std::cerr << "\t| Error! Can't download " << given << ". Perhaps the file is too large." << std::endl; return ""; }
					std::cout << "\tSuccessfully downloaded." << std::endl;
					return file_path;
				}
				else { std::cerr << "\t| Error! Can't download " << given << ". Error creating new file." << std::endl; curl_easy_cleanup(curl); }
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