/*

╔════╗╔═══╗╔╗   ╔═══╗╔═══╗╔═══╗╔═══╗╔══╗
║╔╗╔╗║║╔══╝║║   ║╔══╝║╔═╗║║╔═╗║║╔═╗║║╔╗║  C++11 Telegram Bot API
╚╝║║╚╝║╚══╗║║   ║╚══╗║║ ╚╝║╚═╝║║║ ║║║╚╝╚╗ version 0.5
  ║║  ║╔══╝║║ ╔╗║╔══╝║║╔═╗║╔╗╔╝║╚═╝║║╔═╗║ https://github.com/krupakov/telegrab
  ║║  ║╚══╗║╚═╝║║╚══╗║╚╩═║║║║╚╗║╔═╗║║╚═╝║
  ╚╝  ╚═══╝╚═══╝╚═══╝╚═══╝╚╝╚═╝╚╝ ╚╝╚═══╝

MIT License

Copyright (c) 2020 Gleb Krupakov.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
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
	int chat_id;
	int message_id;
	std::string photo;
	std::string video;
	std::string document;
	std::string text;
	std::string audio;
	std::string sticker;
	std::string voice;
	std::string caption;
	std::vector<std::string> commands;
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
	void send(content message, int chat_id, int reply_to_message_id = 0);
	void forward(int message_id, int chat_id_from, int chat_id_to);
	std::string download(std::string file_id);
	void start();
private:
	int interval;
	int retryTimeout;
	int last_update_id;
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
	bool sendFile(std::string name, std::string text, int chat_id, int type, bool caption, int reply_to_message_id);
};

Telegrab::Telegrab(std::string str)
{
	if (str.find(".json") != std::string::npos)
	{
		config_filename = str;
		std::ifstream file(str);
		if (file.is_open())
		{
			json config = json::parse(file);
			last_update_id = config["last_update_id"];
			interval = config["polling"]["interval"];
			retryTimeout = config["polling"]["retryTimeout"];

			std::string temp = config["token"];
			basic_url = "https://api.telegram.org/bot" + temp;
			download_url = "https://api.telegram.org/file/bot" + temp + "/";
			file.close();
		}
		else std::cout << "\t| Error! Can't open " << str << "." << std::endl;
	}
	else
	{
		config_filename = str + ".json";
		std::ifstream file(config_filename);
		if (file.is_open())
		{
			json config = json::parse(file);
			last_update_id = config["last_update_id"];
			interval = config["polling"]["interval"];
			retryTimeout = config["polling"]["retryTimeout"];
			file.close();
		}
		else
		{
			std::ofstream file(config_filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			json temp;
			temp["token"] = str;
			temp["last_update_id"] = 0; last_update_id = 0;
			temp["polling"]["interval"] = 0; interval = 0;
			temp["polling"]["retryTimeout"] = 10; retryTimeout = 10;
			file << temp;
			file.close();
		}
		basic_url = "https://api.telegram.org/bot" + str;
		download_url = "https://api.telegram.org/file/bot" + str + "/";
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriter);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
}
Telegrab::~Telegrab()
{
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}
bool Telegrab::waitForUpdates()
{
	cleardata();
	std::string url = basic_url + "/getUpdates";
	if (curl)
	{
		buffer = "";
		curl_easy_setopt(curl, CURLOPT_POST, 0);
    	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	    res = curl_easy_perform(curl);
	    if (res != CURLE_OK)
	    {
	    	std::cout << "\t| Error! Can't get updates." << std::endl;
	    	return false;
	    }
	    json file = json::parse(buffer);
		if (file["ok"] && !file["result"].empty())
		{
			for (const auto& element:file["result"])
			{
				if (element["update_id"] <= last_update_id)
					continue;
				std::cout << "\tNew message from " << element["message"]["from"]["first_name"];
				std::cout << "(" << element["message"]["chat"]["id"] << ")." << std::endl;
				last_update_id = element["update_id"];
			    data.chat_id = element["message"]["chat"]["id"];
			    data.message_id = element["message"]["message_id"];
			    if (element["message"].count("photo") != 0)
			    	for (const auto& image:element["message"]["photo"])
			    	{
			    		if (image.count("file_size") != 0)
				    		if (image["file_size"] > 20900000)
				    			break;
			    		data.photo = image["file_id"];
			    	}
			    else data.photo = "";
			    if (element["message"].count("video") != 0)
			    	data.video = element["message"]["video"]["file_id"];
			    else data.video = "";
			    if (element["message"].count("document") != 0)
			    	data.document = element["message"]["document"]["file_id"];
			    else data.document = "";
			    if (element["message"].count("text") != 0)
			    	data.text = element["message"]["text"];
			    else data.text = "";
			    if (element["message"].count("audio") != 0)
			    	data.audio = element["message"]["audio"]["file_id"];
			    else data.audio = "";
			    if (element["message"].count("sticker") != 0)
			    	data.sticker = element["message"]["sticker"]["file_id"];
			    else data.sticker = "";
			    if (element["message"].count("voice") != 0)
			    	data.voice = element["message"]["voice"]["file_id"];
			    else data.voice = "";
			    if (element["message"].count("caption") != 0)
			    	data.caption = element["message"]["caption"];
			    else data.caption = "";
			    if (element["message"].count("entities") != 0)
			    {
			    	int k = 0;
			    	for (const auto& command:element["message"]["entities"])
			    	{
			    		data.commands.push_back("");
			    		int t1 = command["offset"];
			    		int t2 = command["length"];
				    	for (int i = t1; i < (t1 + t2); i++)
				    		data.commands[k] += data.text[i];
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
	std::cout << "\t| Error! Something wrong with cURL." << std::endl;
	return false;
}
void Telegrab::send(content message, int chat_id, int reply_to_message_id)
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
			std::cout << "\t| Error! Can't send a text message to " << chat_id  << "." << std::endl;
	}
}
void Telegrab::forward(int message_id, int chat_id_from, int chat_id_to)
{
	std::cout << "\tForwarding the message " << message_id << " to " << chat_id_to << "..." << std::endl;
	std::string url = basic_url + "/forwardMessage";
	std::string post_url = "chat_id=" + std::to_string(chat_id_to) + "&from_chat_id=" + std::to_string(chat_id_from) + "&message_id=" + std::to_string(message_id);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			std::cout << "\t| Error! Can't forward a message " << message_id << " to " << chat_id_to  << "." << std::endl;
}
bool Telegrab::sendFile(std::string name, std::string text, int chat_id, int type, bool caption, int reply_to_message_id)
{
	bool cap = caption;
	std::string url;
	if (name.find(".") != std::string::npos)
	{
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
				{
					curl_mime_name(field, "photo");
					url = basic_url + "/sendPhoto";
					break;
				}
				case 2:
				{
					curl_mime_name(field, "video");
					url = basic_url + "/sendVideo";
					break;
				}
				case 3:
				{
					curl_mime_name(field, "document");
					url = basic_url + "/sendDocument";
					break;
				}
				case 4:
				{
					curl_mime_name(field, "audio");
					url = basic_url + "/sendAudio";
					break;
				}
				case 5:
				{
					curl_mime_name(field, "sticker");
					url = basic_url + "/sendSticker";
					break;
				}
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
				std::cout << "\t| Error! Can't send a file to " << chat_id  << ". Perhaps the file is too large." << std::endl;

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
			{
				url = basic_url + "/sendPhoto";
				post_url += "&photo=" + name;
				break;
			}
			case 2:
			{
				url = basic_url + "/sendVideo";
				post_url += "&video=" + name;
				break;
			}
			case 3:
			{
				url = basic_url + "/sendDocument";
				post_url += "&document=" + name;
				break;
			}
			case 4:
			{
				url = basic_url + "/sendAudio";
				post_url += "&audio=" + name;
				break;
			}
			case 5:
			{
				url = basic_url + "/sendSticker";
				post_url += "&sticker=" + name;
				break;
			}
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
			std::cout << "\t| Error! Can't send a file to " << chat_id  << "." << std::endl;
	}
	return cap;
}
std::string Telegrab::download(std::string file_id)
{
	std::cout << "\tTrying to download " << file_id << "..." << std::endl;
	buffer = "";
	std::string url = basic_url + "/getFile", post_url = "file_id=" + file_id;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		std::cout << "\t| Error! Can't get a file_path to download the file." << std::endl;
		return "";
	}
	json file = json::parse(buffer);
	if (file["ok"])
	{
		std::string file_path, name, newdir;
		file_path = file["result"]["file_path"];
		url = download_url + file_path;
		for (int i = file_path.size(); i >= 0; i--)
			if (file_path[i] != '/')
				name += file_path[i];
			else break;
		for (int i = 0; i < file_path.size(); i++)
			if (file_path[i] != '/')
				newdir += file_path[i];
			else break;
		std::reverse(name.begin(), name.end());
		name = newdir + "/" + name;
		int err = chmod(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (err == -1)
			err = mkdir(newdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (err != -1)
		{
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_POST, 0);
			std::ofstream file(name, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			if (file.is_open())
			{
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlFileWriter);
				res = curl_easy_perform(curl);
				file.close();
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriter);
				if (res != CURLE_OK)
				{
					std::cout << "\t| Error! Can't download " << file_id << ". Perhaps the file is too large." << std::endl;
					return "";
				}
				std::cout << "\tSuccessfully downloaded." << std::endl;
				return name;
			}
			else std::cout << "\t| Error! Can't download " << file_id << ". Error creating new file." << std::endl;
		}
		else std::cout << "\t| Error! Can't create a folder for the file." << std::endl;
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
	data.commands.clear();
}
void Telegrab::start()
{
	if (!basic_url.empty())
	{
		std::cout << "\tThe bot is running...\n\tChecking for updates..." << std::endl;
		while (1)
		{
			if (interval > 0)
				std::this_thread::sleep_for(std::chrono::seconds(interval));
			if (!waitForUpdates())
			{
				std::cout << "\tReconnecting in " << retryTimeout << " seconds." << std::endl;
				if (retryTimeout > 0)
					std::this_thread::sleep_for(std::chrono::seconds(retryTimeout));
			}
		}
	}
}