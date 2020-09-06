Telegrab is easy-to-use C++11 library for Telegram Bot API.

The library itself consists of a single header file [telegrab.hpp](https://github.com/krupakov/telegrab/blob/master/telegrab.hpp).

* [Dependencies](#dependencies)
* [Compilation](#compilation)
* [Examples](#examples)
* [Basic methods and data](#basic-methods-and-data)

# Dependencies

Telegrab requires the [nlohmann::json](https://github.com/nlohmann/json) and [yhirose::cpp-httplib](https://github.com/yhirose/cpp-httplib) libraries.

Those are single-header libraries. Just drop them into the same folder as the [telegrab.hpp](https://github.com/krupakov/telegrab/blob/master/telegrab.hpp) file.

You also need to install [openssl](https://github.com/openssl/openssl).

NOTE: cpp-httplib currently supports only version 1.1.1.

```
sudo apt install build-essential -y
wget -c https://www.openssl.org/source/openssl-1.1.1d.tar.gz
tar -xzvf openssl-1.1.1d.tar.gz
cd openssl-1.1.1d
./config
make
sudo make install
```

# Compilation

Compile using g++ version 4.8 or higher:

`g++ -std=c++11 main.cpp -lssl -lcrypto -pthread`

# Examples

First you need to include [telegrab.hpp](https://github.com/krupakov/telegrab/blob/master/telegrab.hpp) to your project.

Usually **main.cpp** file will look like this:

```C++
#include "telegrab.hpp"

void Telegrab::Instructions(incoming data)
{
  // Instructions for the bot
}

int main()
{
  Telegrab bot("123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11");
  bot.start();

  return 0;
}
```

You can also use a **json** file as an argument

(if you use a *token*, the config file will be generated automatically).

```C++
Telegrab bot("config.json");
```

Usually **config.json** file will look like this:

```json
{
  "polling":
  {
    "interval":0,
    "limit":100,
    "timeout":30,
    "retryTimeout":10
  },
  "token":"123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11"
}
```
In which `token` - Telegram Bot API token.

`interval` - How often check updates (seconds).

`limit` - Limits the number of updates to be retrieved (1-100).

`timeout` - Timeout in seconds for long polling (0 - short polling).

`retryTimeout` - Reconnecting timeout (seconds).

### Simple echo bot

```C++
void Telegrab::Instructions(incoming data)
{
  if (!data.text.empty())
  {
    content message;
    message.text = data.text;
    // Send (message_id) to (chat_id)
    send(message, data.chat_id);
  }
  ...
}
```

### Simple echo bot that forwards a message

```C++
void Telegrab::Instructions(incoming data)
{
  if (!data.text.empty())
  {
    int message_id = data.message_id;
    int chat_id_from = data.chat_id;
    int chat_id_to = data.chat_id;
    // Forward (message_id) from (chat_id_from) to (chat_id_to)
    forward(message_id, chat_id_from, chat_id_to);
  }
  ...
}
```

### Simple echo bot with a file

How you do this depends on whether you want to download the file or not:

```C++
void Telegrab::Instructions(incoming data)
{
  if (!data.photo.empty())
  {
    content message;
    // Using file_id of the original image (without downloading)
    message.photo = data.photo;
    message.text = "This photo was sent by its file_id";
    send(message, data.chat_id);

    // Using the path to the downloaded image
    message.photo = download(data.photo);
    message.text = "This photo was downloaded";
    send(message, data.chat_id);
  }
  ...
}
```

You can also send your own file, using the full path or URL:

```C++
void Telegrab::Instructions(incoming data)
{
  content message;
  // Upload local file
  message.photo = "photos/image.jpg";
  send(message, data.chat_id);

  // Download and send image
  message.photo = download("https://something.com/image.jpg");
  send(message, data.chat_id);

  // Send image without downloading using Telegram server (5 MB max size for photos and 20 MB max for other types of content)
  message.photo = "https://something.com/image.jpg";
  send(message, data.chat_id);

  ...
}
```

### Message reply

```C++
void Telegrab::Instructions(incoming data)
{
  // If the message is a reply, you can pass the ID of the original message as an argument
  if (data.text == "reply test")
  {
    content message;
    message.text = "This message is a reply";
    // Reply to (message_id) in (chat_id) with (message)
    send(message, data.chat_id, data.message_id);

    return;
  }
  ...
}
```

### How to use special objects (commands, hashtags, etc.)

```C++
void Telegrab::Instructions(incoming data)
{
  for (const auto& entity:data.entities)
  {
    if (entity == "/start")
    {
      content message;
      message.text = "Hello world!";
      send(message, data.chat_id);

      return;
    }
    if (entity == "#example")
    {
      content message;
      message.text = "Example!";
      send(message, data.chat_id);

      return;
    }
  }
  ...
}
```

### Creating a custom reply keyboard

```C++
void Telegrab::Instructions(incoming data)
{
  for (const auto& entity:data.entities)
  {
    if (entity == "/start")
    {
      // The Telegram keyboard consists of rows, which consist of buttons
      ReplyKeyboardRow row_1;

      // Creating button
      KeyboardButton btn;

      // Button text
      btn.text = "Click me";

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

      send(message, data.chat_id);

      return;
    }
  }
  ...
}
```

### How to hide a custom keyboard

```C++
void Telegrab::Instructions(incoming data)
{
  if (data.text.find("Click me") != std::string::npos)
  {
    ontent message;
    message.text = "The custom keyboard has been removed.";

    // Deleting custom keyboard
    ReplyKeyboardHide h;
    h.hide = true;
    // Optional field, 'false' by default
    h.selective = false;
    message.hide_reply_keyboard = h;

    send(message, data.chat_id);

    return;
  }
  ...
}
```

### All instructions must be in the same method

```C++
void Telegrab::Instructions(incoming data)
{
  if (!data.sticker.empty())
  {
    content message;
    message.sticker = "CAADAgADSAoAAm4y2AABrGwuPYwIwBwWBA";
    send(message, data.chat_id);

    return;
  }
  if (!data.document.empty())
  {
    content message;
    message.document = download(data.document);
    send(message, data.chat_id);

    return;
  }
  ...
}
```

# Basic methods and data

## Data

### Incoming message

Integer:

`chat_id` and `message_id`

String (text or filename or file_id):

`photo`

`video`

`document`

`text`

`audio`

`sticker`

`voice`

`caption`

Vector<*string*> (contains all entities from the message, i.e. commands, hashtags, etc.):

`entities`

### Upcoming message

String (text or filename or file_id or URL):

`photo`

`video`

`document`

`text`

`audio`

`sticker`

`reply_keyboard`

`hide_reply_keyboard`

## Methods

### Send

Send a message (if the message is a reply, 3d parameter required)

`void send(content message, int chat_id)`

`void send(content message, int chat_id, int message_id)`

### Forward

Forward a message

`void forward(int message_id, int chat_id_from, int chat_id_to)`

### Download

Download a file (returns the path to the file with the name included)

`string download(string given)`
