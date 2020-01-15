/*
╔════╗╔═══╗╔╗   ╔═══╗╔═══╗╔═══╗╔═══╗╔══╗
║╔╗╔╗║║╔══╝║║   ║╔══╝║╔═╗║║╔═╗║║╔═╗║║╔╗║  C++11 Telegram Bot API
╚╝║║╚╝║╚══╗║║   ║╚══╗║║ ╚╝║╚═╝║║║ ║║║╚╝╚╗ version 0.9.2
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
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <sys/stat.h>
#include "json.hpp"
using json = nlohmann::json;

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
	std::string download(std::string given);
	void start();
private:
	unsigned short int limit;
	unsigned short int interval;
	unsigned short int timeout;
	unsigned short int retryTimeout;
	unsigned int last_update_id;
	std::string basic_url;
	std::string download_url;
	std::string buffer;
	incoming data;

	CURL *curl;
	CURLcode res;

	std::string config_filename;

	void Instructions();
	void cleardata();
	bool waitForUpdates();
	bool sendFile(std::string name, std::string text, unsigned int chat_id, unsigned char type, bool caption, unsigned int reply_to_message_id);

	bool fatalError;
};

Telegrab::Telegrab(std::string str):fatalError(false)
{
	if (str.find(".json") != std::string::npos)
	{
		config_filename = str;
		std::ifstream file(str);
		if (file.is_open())
		{
			json config = json::parse(file);
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
			{ std::cerr << "\t| Error! Can't open " << str << "." << std::endl; fatalError = true; }
	}
	else
	{
		config_filename = str + ".json";
		std::ifstream file(config_filename);
		if (file.is_open())
		{
			json config = json::parse(file);
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
				json temp;
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
				{ std::cerr << "\t| Error! Unable to create config file." << std::endl; fatalError = true; }
		}
		basic_url = "https://api.telegram.org/bot" + str;
		download_url = "https://api.telegram.org/file/bot" + str + "/";
	}
	short int err = chmod("downloads", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (err == -1)
		if (mkdir("downloads", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{ std::cerr << "\t| Error! Unable to create download folder." << std::endl; fatalError = true; }

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriter);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	if (!curl)
		fatalError = true;
}
Telegrab::~Telegrab()
{
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}
bool Telegrab::waitForUpdates()
{
	cleardata();
	buffer = "";
	std::string current;
	std::string url = basic_url + "/getUpdates";
	std::string post_url = "limit=" + std::to_string(limit);
	if (timeout > 0)
		post_url += "&timeout=" + std::to_string(timeout);
	if (last_update_id > 0)
		post_url += "&offset=" + std::to_string(last_update_id + 1);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		{ std::cerr << "\t| Error! Can't get updates." << std::endl; return false; }
	json file = json::parse(buffer);
	if (file["ok"] && !file["result"].empty())
	{
		for (const auto& element:file["result"])
		{
			current = "message";
			if (element.count("edited_message") != 0)
				current = "edited_message";
			std::cout << "\tNew message from " << element[current]["from"]["first_name"];
			std::cout << "(" << element[current]["chat"]["id"] << ")." << std::endl;
			last_update_id = element["update_id"];
			data.chat_id = element[current]["chat"]["id"];
			data.message_id = element[current]["message_id"];
			if (element[current].count("photo") != 0)
				for (const auto& image:element[current]["photo"])
				{
					if (image.count("file_size") != 0)
						if (image["file_size"] > 20900000)
							break;
					data.photo = image["file_id"];
				}
			else data.photo = "";
			if (element[current].count("video") != 0)
				data.video = element[current]["video"]["file_id"];
			else data.video = "";
			if (element[current].count("document") != 0)
				data.document = element[current]["document"]["file_id"];
			else data.document = "";
			if (element[current].count("text") != 0)
				data.text = element[current]["text"];
			else data.text = "";
			if (element[current].count("audio") != 0)
				data.audio = element[current]["audio"]["file_id"];
			else data.audio = "";
			if (element[current].count("sticker") != 0)
				data.sticker = element[current]["sticker"]["file_id"];
			else data.sticker = "";
			if (element[current].count("voice") != 0)
				data.voice = element[current]["voice"]["file_id"];
			else data.voice = "";
			if (element[current].count("caption") != 0)
				data.caption = element[current]["caption"];
			else data.caption = "";
			if (element[current].count("entities") != 0)
			{
				unsigned short int k = 0;
				for (const auto& entity:element[current]["entities"])
				{
					data.entities.push_back("");
					unsigned short int t1 = entity["offset"];
					unsigned short int t2 = entity["length"];
					for (unsigned short int i = t1; i < (t1 + t2); i++)
						data.entities[k] += data.text[i];
					k++;
				}
			}
			Instructions();
		}
		std::ifstream file(config_filename);
		if (file.is_open())
		{
			json config = json::parse(file);
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
	std::cout << "\tSending a message to " << chat_id << "..." << std::endl;
	bool caption = false;
	std::string url, post_url;
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
		url = basic_url + "/sendMessage";
		post_url = "chat_id=" + std::to_string(chat_id) + "&text=" + message.text;
		if (reply_to_message_id != 0)
			post_url += "&reply_to_message_id=" + std::to_string(reply_to_message_id);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			std::cerr << "\t| Error! Can't send a text message to " << chat_id  << "." << std::endl;
	}
}
void Telegrab::forward(unsigned int message_id, unsigned int chat_id_from, unsigned int chat_id_to)
{
	std::cout << "\tForwarding the message " << message_id << " to " << chat_id_to << "..." << std::endl;
	std::string url = basic_url + "/forwardMessage";
	std::string post_url = "chat_id=" + std::to_string(chat_id_to) + "&from_chat_id=" + std::to_string(chat_id_from) + "&message_id=" + std::to_string(message_id);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			std::cerr << "\t| Error! Can't forward a message " << message_id << " to " << chat_id_to  << "." << std::endl;
}
bool Telegrab::sendFile(std::string name, std::string text, unsigned int chat_id, unsigned char type, bool caption, unsigned int reply_to_message_id)
{
	bool cap = caption;
	std::string url;
	std::ifstream file(name);
	if (file.is_open())
	{
		file.close();
		CURL *curl_multipart;
		curl_mime *form = NULL;
		curl_mimepart *field = NULL;

		curl_multipart = curl_easy_init();
		curl_easy_setopt(curl_multipart, CURLOPT_SSL_VERIFYPEER, 0L);
		/* We need to set up a CURLOPT_WRITEFUNCTION to make it not use stdout */
		curl_easy_setopt(curl_multipart, CURLOPT_WRITEDATA, &buffer);
		curl_easy_setopt(curl_multipart, CURLOPT_WRITEFUNCTION, curlWriter);
		if (curl_multipart)
		{
			/* Create the form */ 
			form = curl_mime_init(curl_multipart);
			/* Fill in the file upload field */
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
			/* Fill in the chat_id field */
			field = curl_mime_addpart(form);
			curl_mime_name(field, "chat_id");
			curl_mime_data(field, std::to_string(chat_id).c_str(), CURL_ZERO_TERMINATED);
			if (!text.empty() && !caption && type != 5)
			{
				/* Fill in the caption field (if message contains text) */
				field = curl_mime_addpart(form);
				curl_mime_name(field, "caption");
				curl_mime_data(field, text.c_str(), CURL_ZERO_TERMINATED);
				cap = true;
			}
			/* Fill in the reply_to_message_id field */
			if (reply_to_message_id != 0)
			{
				field = curl_mime_addpart(form);
				curl_mime_name(field, "reply_to_message_id");
				curl_mime_data(field, std::to_string(reply_to_message_id).c_str(), CURL_ZERO_TERMINATED);
			}
			/* what URL that receives this POST */ 
			curl_easy_setopt(curl_multipart, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl_multipart, CURLOPT_MIMEPOST, form);
			res = curl_easy_perform(curl_multipart);
			if (res != CURLE_OK)
				std::cerr << "\t| Error! Can't send a file to " << chat_id  << ". Perhaps the file is too large." << std::endl;

			/* clear buffer */ 
			buffer = "";
			/* always cleanup */ 
			curl_easy_cleanup(curl_multipart);
			/* then cleanup the form */ 
			curl_mime_free(form);
		}
	}
	else
	{
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
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			std::cerr << "\t| Error! Can't send " << name << " to " << chat_id  << "." << std::endl;
	}
	return cap;
}
std::string Telegrab::download(std::string given)
{
	std::cout << "\tTrying to download " << given << "..." << std::endl;
	buffer = "";
	if (given.empty())
		{ std::cerr << "\t| Error! Given string is empty." << std::endl; return ""; }
	if (given.find(".") != std::string::npos)
	{
		std::string file_path;
		unsigned short int index = given.size() - 1;
		if (given[index] == '/')
			index--;
		for (index; index >= 0; index--)
			if (given[index] != '/')
				file_path += given[index];
			else break;
		std::reverse(file_path.begin(), file_path.end());
		file_path = "downloads/" + file_path;
		curl_easy_setopt(curl, CURLOPT_URL, given.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 0);
		std::ofstream file(file_path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
		if (file.is_open())
		{
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlFileWriter);
			res = curl_easy_perform(curl);
			file.close();
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriter);
			if (res != CURLE_OK)
				{ std::cerr << "\t| Error! Can't download " << given << "." << std::endl; return ""; }
			std::cout << "\tSuccessfully downloaded." << std::endl;
			return file_path;
		}
	}
	else
	{
		std::string url = basic_url + "/getFile", post_url = "file_id=" + given;
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			{ std::cerr << "\t| Error! Can't get a file_path to download the file." << std::endl; return ""; }
		json result = json::parse(buffer);
		if (result["ok"])
		{
			std::string file_path, newdir = "downloads/";
			file_path = result["result"]["file_path"];
			url = download_url + file_path;
			for (unsigned short int i = 0; i < file_path.size(); i++)
				if (file_path[i] != '/')
					newdir += file_path[i];
				else break;
			file_path = "downloads/" + file_path;
			short int err = chmod(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (err == -1)
				err = mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (err != -1)
			{
				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_POST, 0);
				std::ofstream file(file_path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
				if (file.is_open())
				{
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlFileWriter);
					res = curl_easy_perform(curl);
					file.close();
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
					curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriter);
					if (res != CURLE_OK)
						{ std::cerr << "\t| Error! Can't download " << given << ". Perhaps the file is too large." << std::endl; return ""; }
					std::cout << "\tSuccessfully downloaded." << std::endl;
					return file_path;
				}
				else std::cerr << "\t| Error! Can't download " << given << ". Error creating new file." << std::endl;
			}
			else std::cerr << "\t| Error! Can't create a folder for the file." << std::endl;
		}
	}
	return "";
}
void Telegrab::cleardata()
{
	data.photo = "";
	data.video = "";
	data.document = "";
	data.text = "";
	data.audio = "";
	data.sticker = "";
	data.voice = "";
	data.caption = "";
	data.entities.clear();
}
void Telegrab::start()
{
	if (!fatalError && !basic_url.empty())
	{
		std::cout << "\tChecking for updates..." << std::endl;
		while (true)
		{
			if (!waitForUpdates())
			{
				std::cerr << "\t| Error! Failed to connect. Reconnecting in " << retryTimeout << " seconds..." << std::endl;
				if (retryTimeout > 0)
					std::this_thread::sleep_for(std::chrono::seconds(retryTimeout));
			}
			if (interval > 0)
				std::this_thread::sleep_for(std::chrono::seconds(interval));
		}
	}
}