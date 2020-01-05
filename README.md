Telegrab is easy-to-use C++11 library for Telegram Bot API.

The library itself consists of a single header file [telegrab.hpp](https://github.com/krupakov/telegrab/blob/master/telegrab.hpp).

* Third-party libraries
* Some examples
* Launch
* Basic methods and data types

# Third-party libraries

Telegrab requires the [nlohmann::json](https://github.com/nlohmann/json), [openssl](https://github.com/openssl/openssl) and [cURL](https://github.com/curl/curl) libraries.

Make sure to download and install these before launching.

# Some examples

First you need to include [telegrab.hpp](https://github.com/krupakov/telegrab/blob/master/telegrab.hpp) to your **main.cpp** file.

Usually **main.cpp** file will look like this:

```C++
#include "telegrab.hpp"

void Telegrab::Instructions()
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
  "token":"bot_token",
  "polling":
  {
    "interval":0,
    "limit":100,
    "timeout":30,
    "retryTimeout":30
  },
  "last_update_id":0
}
```
In which `token` - Telegram Bot API token.

`interval` - How often check updates (seconds).

`limit` - Limits the number of updates to be retrieved (1-100).

`timeout` - Timeout in seconds for long polling. Defaults to 0, i.e. usual short polling.

`retryTimeout` - Reconnecting timeout (seconds). 

`last_update_id` - Must be 0 at the first launch

### Simple echo bot

```C++
void Telegrab::Instructions()
{
  if (!data.text.empty())
  {
    content message;
    message.text = data.text;
    // Send (message_id) to (chat_id)
    send(message, data.chat_id);
  }
}
```

### Simple echo bot that forwards a message

```C++
void Telegrab::Instructions()
{
  if (!data.text.empty())
  {
    int message_id = data.message_id;
    int chat_id_from = data.chat_id;
    int chat_id_to = data.chat_id;
    // Forward (message_id) from (chat_id_from) to (chat_id_to)
    forward(message_id, chat_id_from, chat_id_to);
  }
}
```

### Simple echo bot with an image

```C++
void Telegrab::Instructions()
{
  if (!data.photo.empty())
  {
    content message;
    // How you do this depends on whether you want to download the file or not
    // Using file_id of the original image (preferred)
    message.photo = data.photo;
    message.text = "This photo was sent by its file_id";
    send(message, data.chat_id);

    // Using the name of the downloaded image
    message.photo = download(data.photo);
    message.text = "This photo was downloaded";
    send(message, data.chat_id);
  }
}
```

You can also send your own image, using the full path to the file as an argument:

```C++
message.photo = "image.jpg";
send(message, data.chat_id);
```

### Message reply

```C++
void Telegrab::Instructions()
{
  // If the message is a reply, you can pass the ID of the original message as an argument
  if (data.text == "reply")
  {
    content message;
    message.text = "This message is a reply";
    // Reply to (message_id) in (chat_id) with (message)
    send(message, data.chat_id, data.message_id);
  }
}
```

### How to use commands

```C++
void Telegrab::Instructions()
{
  for (auto command = data.commands.begin(); command != data.commands.end(); ++command)
  {
    if (*command == "/start")
    {
      content message;
      message.text = "Hello world!";
      send(message, data.chat_id);
    }
  }
}
```

### All instructions must be in the same method

```C++
void Telegrab::Instructions()
{
  if (!data.sticker.empty())
  {
    content message;
    message.sticker = "CAADAgADSAoAAm4y2AABrGwuPYwIwBwWBA";
    send(message, data.chat_id);
  }
  if (!data.document.empty())
  {
    content message;
    message.document = download(data.document);
    send(message, data.chat_id);
  }
}
```

# Launch

Install **build-essential** first:

`sudo apt-get install build-essential -y`

Compilation:

`g++ -std=c++11 main.cpp -lcurl`

How to start a process:

`nohup ./a.out &`

How to kill a process:

`kill PID`

**PID** you can find using the `ps aux` command.

# Basic methods and data types

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

Vector<*string*> (contains all entities from the message, i.e. commands, hashtags etc.):

`commands`

### Upcoming message

String (text or filename or file_id):

`photo`

`video`

`document`

`text`

`audio`

`sticker`

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

`string download(string file_id)`
