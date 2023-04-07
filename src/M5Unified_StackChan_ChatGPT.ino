// #include <FS.h>
#include <SD.h>
#include <SPIFFS.h>
#include <M5Unified.h>
#include <nvs.h>
#include <Avatar.h>

#include <AudioOutput.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include "AudioFileSourceVoiceTextStream.h"
#include "AudioOutputM5Speaker.h"
#include <ServoEasing.hpp> // https://github.com/ArminJo/ServoEasing

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCACertificate.h"
#include <ArduinoJson.h>
#include <ESP32WebServer.h>
#include <ESPmDNS.h>

#include <Adafruit_NeoPixel.h>

#define USE_SDCARD
#define WIFI_SSID "SET YOUR WIFI SSID"
#define WIFI_PASS "SET YOUR WIFI PASS"
#define OPENAI_APIKEY "SET YOUR OPENAI APIKEY"
#define VOICETEXT_APIKEY "SET YOUR VOICETEXT APIKEY"

#define PIN 25      // GPIO25でLEDを使用する
#define NUM_LEDS 10 // LEDの数を指定する

Adafruit_NeoPixel pixels(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800); // 800kHzでNeoPixelを駆動 おまじない行

#define USE_SERVO
#ifdef USE_SERVO
#if defined(ARDUINO_M5STACK_Core2)
#define SERVO_PIN_X 13
#define SERVO_PIN_Y 14
#elif defined(ARDUINO_M5STACK_FIRE)
#include <M5Stack.h>
#define SERVO_PIN_X 21
#define SERVO_PIN_Y 22
#elif defined(ARDUINO_M5Stack_Core_ESP32)
#include <M5Stack.h>
#define SERVO_PIN_X 21
#define SERVO_PIN_Y 22
#endif
#endif

using namespace m5avatar;
Avatar avatar;
const Expression expressions_table[] = {
    Expression::Neutral,
    Expression::Happy,
    Expression::Sleepy,
    Expression::Doubt,
    Expression::Sad,
    Expression::Angry};

ESP32WebServer server(80);

String OPENAI_API_KEY = "";

const char *text1 = "みなさんこんにちは、私の名前はスタックチャンです、よろしくね。";
const char *tts_parms1 = "&emotion_level=4&emotion=happiness&format=mp3&speaker=takeru&volume=200&speed=100&pitch=130"; // he has natural(16kHz) mp3 voice
const char *tts_parms2 = "&emotion=happiness&format=mp3&speaker=hikari&volume=200&speed=120&pitch=130";                 // he has natural(16kHz) mp3 voice
const char *tts_parms3 = "&emotion=anger&format=mp3&speaker=bear&volume=200&speed=120&pitch=100";                       // he has natural(16kHz) mp3 voice
const char *tts_parms4 = "&emotion_level=2&emotion=happiness&format=mp3&speaker=haruka&volume=200&speed=80&pitch=70";
const char *tts_parms5 = "&emotion_level=4&emotion=happiness&format=mp3&speaker=santa&volume=200&speed=120&pitch=90";
const char *tts_parms6 = "&emotion=happiness&format=mp3&speaker=hikari&volume=150&speed=110&pitch=140"; // he has natural(16kHz) mp3 voice
const char *tts_parms_table[6] = {tts_parms1, tts_parms2, tts_parms3, tts_parms4, tts_parms5};
int tts_parms_no = 1;

#include <deque>

// 保存する質問と回答の最大数
const int MAX_HISTORY = 3;

// 過去の質問と回答を保存するデータ構造
std::deque<String> chatHistory;

// テキストデータを句読点単位で分割する関数

bool isPunctuation(char16_t c) {
  return c == u'、' || c == u'。';
}

std::vector<String> splitText(const String& text) {
  std::vector<String> segments;
  String segment = "";
  for (int i = 0; i < text.length(); i++) {
    char16_t c = text.charAt(i);
    segment += String(c);
    if (isPunctuation(c)) {
      segments.push_back(segment);
      segment = "";
    }
  }
  if (segment != "") {
    segments.push_back(segment);
  }
  return segments;
}

// C++11 multiline string constants are neato...
static const char HEAD[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="UTF-8">
  <title>AIｽﾀｯｸﾁｬﾝ</title>
</head>)KEWL";

static const char APIKEY_HTML[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>APIキー設定</title>
  </head>
  <body>
    <h1>APIキー設定</h1>
    <form>
      <label for="role1">OpenAI API Key</label>
      <input type="text" id="openai" name="openai" oninput="adjustSize(this)"><br>
      <label for="role2">VOIVETEXT API Key</label>
      <input type="text" id="voicetext" name="voicetext" oninput="adjustSize(this)"><br>
      <button type="button" onclick="sendData()">送信する</button>
    </form>
    <script>
      function adjustSize(input) {
        input.style.width = ((input.value.length + 1) * 8) + 'px';
      }
      function sendData() {
        // FormDataオブジェクトを作成
        const formData = new FormData();

        // 各ロールの値をFormDataオブジェクトに追加
        const openaiValue = document.getElementById("openai").value;
        if (openaiValue !== "") formData.append("openai", openaiValue);

        const voicetextValue = document.getElementById("voicetext").value;
        if (voicetextValue !== "") formData.append("voicetext", voicetextValue);

	    // POSTリクエストを送信
	    const xhr = new XMLHttpRequest();
	    xhr.open("POST", "/apikey_set");
	    xhr.onload = function() {
	      if (xhr.status === 200) {
	        alert("データを送信しました！");
	      } else {
	        alert("送信に失敗しました。");
	      }
	    };
	    xhr.send(formData);
	  }
	</script>
  </body>
</html>)KEWL";

static const char ROLE_HTML[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html>
<head>
	<title>ロール設定</title>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<style>
		textarea {
			width: 80%;
			height: 200px;
			resize: both;
		}
	</style>
</head>
<body>
	<h1>ロール設定</h1>
	<form onsubmit="postData(event)">
		<label for="textarea">Text Area:</label><br>
		<textarea id="textarea" name="textarea"></textarea><br><br>
		<input type="submit" value="Submit">
	</form>
	<script>
		function postData(event) {
			event.preventDefault();
			const textAreaContent = document.getElementById("textarea").value.trim();
//			if (textAreaContent.length > 0) {
				const xhr = new XMLHttpRequest();
				xhr.open("POST", "/role_set", true);
				xhr.setRequestHeader("Content-Type", "text/plain;charset=UTF-8");
			// xhr.onload = () => {
			// 	location.reload(); // 送信後にページをリロード
			// };
			xhr.onload = () => {
				document.open();
				document.write(xhr.responseText);
				document.close();
			};
				xhr.send(textAreaContent);
//        document.getElementById("textarea").value = "";
				alert("Data sent successfully!");
//			} else {
//				alert("Please enter some text before submitting.");
//			}
		}
	</script>
</body>
</html>)KEWL";
String speech_text = "";
String speech_text_buffer = "";
DynamicJsonDocument chat_doc(1024);
String json_ChatString = "{\"model\": \"gpt-3.5-turbo\",\"messages\": [{\"role\": \"user\", \"content\": \""
                         "\"}]}";
// String json_ChatString =
// "{\"model\": \"gpt-3.5-turbo\",\
  //  \"messages\": [\
  //                 {\"role\": \"user\", \"content\": \"" + text + "\"},\
  //                 {\"role\": \"system\", \"content\": \"あなたは「スタックちゃん」と言う名前の小型ロボットとして振る舞ってください。\"},\
  //                 {\"role\": \"system\", \"content\": \"あなたはの使命は人々の心を癒すことです。\"},\
  //                 {\"role\": \"system\", \"content\": \"幼い子供の口調で話してください。\"},\
  //                 {\"role\": \"system\", \"content\": \"あなたの友達はロボハチマルハチマルさんです。\"},\
  //                 {\"role\": \"system\", \"content\": \"語尾には「だよ｝をつけて話してください。\"}\
  //               ]}";

// init_chat_doc(json_ChatString.c_str());
bool init_chat_doc(const char *data)
{
  DeserializationError error = deserializeJson(chat_doc, data);
  if (error)
  {
    return false;
  }
  String json_str;                         //= JSON.stringify(chat_doc);
  serializeJsonPretty(chat_doc, json_str); // 文字列をシリアルポートに出力する
  Serial.println(json_str);
  return true;
}

void handleRoot()
{
  server.send(200, "text/plain", "hello from m5stack!");
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  //  server.send(404, "text/plain", message);
  server.send(404, "text/html", String(HEAD) + String("<body>") + message + String("</body>"));
}

void handle_speech()
{
  String message = server.arg("say");
  String expression = server.arg("expression");
  String voice = server.arg("voice");
  int expr = 0;
  int parms_no = 1;
  Serial.println(expression);
  if (expression != "")
  {
    expr = expression.toInt();
    if (expr < 0)
      expr = 0;
    if (expr > 5)
      expr = 5;
  }
  if (voice != "")
  {
    parms_no = voice.toInt();
    if (parms_no < 0)
      parms_no = 0;
    if (parms_no > 4)
      parms_no = 4;
  }
  //  message = message + "\n";
  Serial.println(message);
  ////////////////////////////////////////
  // 音声の発声
  ////////////////////////////////////////
  avatar.setExpression(expressions_table[expr]);
  VoiceText_tts((const char *)message.c_str(), tts_parms_table[parms_no]);
  //  avatar.setExpression(expressions_table[0]);
  server.send(200, "text/plain", String("OK"));
}

String https_post_json(const char *url, const char *json_string, const char *root_ca)
{
  String payload = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if (client)
  {
    client->setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
      HTTPClient https;
      //      https.setTimeout( 25000 );
      https.setTimeout(50000);

      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url))
      { // HTTPS
        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Content-Type", "application/json");
        //        https.addHeader("Authorization", "Bearer YOUR_API_KEY");
        https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);
        int httpCode = https.POST((uint8_t *)json_string, strlen(json_string));

        // httpCode will be negative on error
        if (httpCode > 0)
        {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
          {
            payload = https.getString();
          }
        }
        else
        {
          Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
      }
      else
      {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }
    delete client;
  }
  else
  {
    Serial.println("Unable to create client");
  }
  return payload;
}

String chatGpt(String json_string)
{
  String response = "";
  //  String json_string = "{\"model\": \"gpt-3.5-turbo\",\"messages\": [{\"role\": \"user\", \"content\": \"" + text + "\"},{\"role\": \"system\", \"content\": \"あなたは「スタックちゃん」と言う名前の小型ロボットとして振る舞ってください。\"},{\"role\": \"system\", \"content\": \"あなたはの使命は人々の心を癒すことです。\"},{\"role\": \"system\", \"content\": \"幼い子供の口調で話してください。\"}]}";
  avatar.setExpression(Expression::Doubt);
  avatar.setSpeechText("考え中…");

  // LED 3番と7番を黄色に光らせる
  pixels.setPixelColor(2, 255, 255, 255); // 白色
  pixels.setPixelColor(7, 255, 255, 255); // 白色
  pixels.show();

  String ret = https_post_json("https://api.openai.com/v1/chat/completions", json_string.c_str(), root_ca_openai);
  avatar.setExpression(Expression::Neutral);
  avatar.setSpeechText("");
  Serial.println(ret);

  // 音声が再生された後にLEDを消灯
  pixels.setPixelColor(2, 0, 0, 0); // 黒（消灯）
  pixels.setPixelColor(7, 0, 0, 0); // 黒（消灯）
  pixels.show();

  if (ret != "")
  {
    DynamicJsonDocument doc(2000);
    DeserializationError error = deserializeJson(doc, ret.c_str());
    if (error)
    {
      // LED 3番と7番を黄色に光らせる
      pixels.setPixelColor(2, 255, 0, 0); // 赤色
      pixels.setPixelColor(7, 255, 0, 0); // 赤色
      pixels.show();

      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      avatar.setExpression(Expression::Sad);
      avatar.setSpeechText("エラーです");
      response = "エラーです";
      delay(1000);
      avatar.setSpeechText("");
      avatar.setExpression(Expression::Neutral);

      // 音声が再生された後にLEDを消灯
      pixels.setPixelColor(2, 0, 0, 0); // 黒（消灯）
      pixels.setPixelColor(7, 0, 0, 0); // 黒（消灯）
      pixels.show();
    }
    else
    {
      const char *data = doc["choices"][0]["message"]["content"];
      Serial.println(data);
      response = String(data);
      std::replace(response.begin(), response.end(), '\n', ' ');
    }
  }
  else
  {
    // LED 3番と7番を黄色に光らせる
    pixels.setPixelColor(2, 255, 255, 0); // 黄色
    pixels.setPixelColor(7, 255, 255, 0); // 黄色
    pixels.show();

    avatar.setExpression(Expression::Sad);
    avatar.setSpeechText("わかりません");
    response = "わかりません";
    delay(1000);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);

    // 音声が再生された後にLEDを消灯
    pixels.setPixelColor(2, 0, 0, 0); // 黒（消灯）
    pixels.setPixelColor(7, 0, 0, 0); // 黒（消灯）
    pixels.show();
  }
  return response;
}

void handle_chat()
{
  String text = server.arg("text");

  // 質問をチャット履歴に追加
  chatHistory.push_back(text);

  // チャット履歴が最大数を超えた場合、古い質問と回答を削除
  if (chatHistory.size() > MAX_HISTORY * 2)
  {
    chatHistory.pop_front();
    chatHistory.pop_front();
  }

  String messagesJson = "[";
  for (int i = 0; i < chatHistory.size(); i++)
  {
    messagesJson += String("{\"role\": \"") + (i % 2 == 0 ? "user" : "assistant") + "\", \"content\": \"" + chatHistory[i] + "\"}";
    if (i < chatHistory.size() - 1)
    {
      messagesJson += ",";
    }
  }
  messagesJson += "]";

  String json_string = "{\"model\": \"gpt-3.5-turbo\",\"messages\": " + messagesJson + "}";

  String response = chatGpt(json_string);
  speech_text = response;

  // 返答をチャット履歴に追加
  chatHistory.push_back(response);

  // テキストデータを句読点単位で分割し、順次送信
  std::vector<String> segments = splitText(response);
  String htmlResponse = "";
  for (const String &segment : segments)
  {
    htmlResponse += segment;
  }
  server.send(200, "text/html", String(HEAD) + String("<body>") + htmlResponse + String("</body>"));
}

void handle_apikey()
{
  // ファイルを読み込み、クライアントに送信する
  server.send(200, "text/html", APIKEY_HTML);
}

void handle_apikey_set()
{
  // POST以外は拒否
  if (server.method() != HTTP_POST)
  {
    return;
  }
  // openai
  String openai = server.arg("openai");
  // voicetxt
  String voicetext = server.arg("voicetext");

  OPENAI_API_KEY = openai;
  tts_user = voicetext;
  Serial.println(openai);
  Serial.println(voicetext);

  uint32_t nvs_handle;
  if (ESP_OK == nvs_open("apikey", NVS_READWRITE, &nvs_handle))
  {
    nvs_set_str(nvs_handle, "openai", openai.c_str());
    nvs_set_str(nvs_handle, "voicetext", voicetext.c_str());
    nvs_close(nvs_handle);
  }
  server.send(200, "text/plain", String("OK"));
}

void handle_role()
{
  // ファイルを読み込み、クライアントに送信する
  server.send(200, "text/html", ROLE_HTML);
}

bool save_json()
{
  // SPIFFSをマウントする
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  // JSONファイルを作成または開く
  File file = SPIFFS.open("/data.json", "w");
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return false;
  }

  // JSONデータをシリアル化して書き込む
  serializeJson(chat_doc, file);
  file.close();
  return true;
}

/**
 * アプリからテキスト(文字列)と共にRoll情報が配列でPOSTされてくることを想定してJSONを扱いやすい形に変更
 * 出力形式をJSONに変更
 */
void handle_role_set()
{
  // POST以外は拒否
  if (server.method() != HTTP_POST)
  {
    return;
  }
  String role = server.arg("plain");
  if (role != "")
  {
    JsonArray messages = chat_doc["messages"];
    JsonObject systemMessage1 = messages.createNestedObject();
    systemMessage1["role"] = "system";
    systemMessage1["content"] = role;
  }
  else
  {
    init_chat_doc(json_ChatString.c_str());
  }

  // JSONデータをspiffsへ出力する
  save_json();

  // 整形したJSONデータを出力するHTMLデータを作成する
  String html = "<html><body><pre>";
  serializeJsonPretty(chat_doc, html);
  html += "</pre></body></html>";
  // String json_str; //= JSON.stringify(chat_doc);
  // serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
  // Serial.println(json_str);
  //  server.send(200, "text/html", String(HEAD)+String("<body>")+json_str+String("</body>"));

  // HTMLデータをシリアルに出力する
  Serial.println(html);
  server.send(200, "text/html", html);
  //  server.send(200, "text/plain", String("OK"));
};

// 整形したJSONデータを出力するHTMLデータを作成する
void handle_role_get()
{

  String html = "<html><body><pre>";
  serializeJsonPretty(chat_doc, html);
  html += "</pre></body></html>";

  // HTMLデータをシリアルに出力する
  Serial.println(html);
  server.send(200, "text/html", String(HEAD) + html);
};

void handle_role_set1()
{
  // POST以外は拒否
  if (server.method() != HTTP_POST)
  {
    return;
  }

  JsonArray messages = chat_doc["messages"];

  // Roll[1]
  String role1 = server.arg("role1");
  if (role1 != "")
  {
    JsonObject systemMessage1 = messages.createNestedObject();
    systemMessage1["role"] = "system";
    systemMessage1["content"] = role1;
  }
  // Roll[2]
  String role2 = server.arg("role2");
  if (role2 != "")
  {
    JsonObject systemMessage2 = messages.createNestedObject();
    systemMessage2["role"] = "system";
    systemMessage2["content"] = role2;
  }
  // Roll[3]
  String role3 = server.arg("role3");
  if (role3 != "")
  {
    JsonObject systemMessage3 = messages.createNestedObject();
    systemMessage3["role"] = "system";
    systemMessage3["content"] = role3;
  }
  // Roll[4]
  String role4 = server.arg("role4");
  if (role4 != "")
  {
    JsonObject systemMessage4 = messages.createNestedObject();
    systemMessage4["role"] = "system";
    systemMessage4["content"] = role4;
  }
  // Roll[5]
  String role5 = server.arg("role5");
  if (role5 != "")
  {
    JsonObject systemMessage5 = messages.createNestedObject();
    systemMessage5["role"] = "system";
    systemMessage5["content"] = role5;
  }
  // Roll[6]
  String role6 = server.arg("role6");
  if (role6 != "")
  {
    JsonObject systemMessage6 = messages.createNestedObject();
    systemMessage6["role"] = "system";
    systemMessage6["content"] = role6;
  }
  // Roll[7]
  String role7 = server.arg("role7");
  if (role7 != "")
  {
    JsonObject systemMessage7 = messages.createNestedObject();
    systemMessage7["role"] = "system";
    systemMessage7["content"] = role7;
  }
  // Roll[8]
  String role8 = server.arg("role8");
  if (role8 != "")
  {
    JsonObject systemMessage8 = messages.createNestedObject();
    systemMessage8["role"] = "system";
    systemMessage8["content"] = role8;
  }
  /*
    // JSON配列生成
    DynamicJsonDocument response(1024);
    response["status"] = "failed";

    // JSON出力
    String jsonResponse;

    try {
      String json_string;
      serializeJson(chat_doc, json_string);
      message = chatGpt(json_string);
      response["status"] = "success";
      response["youre_message"] = text;
      response["stackchan_message"] = message;
      serializeJson(response, jsonResponse);

      // 音声
      speech_text = message;
    } catch(...) {
      response["message"] = "something wrong.";
      serializeJson(response, jsonResponse);
    }
    server.send(200, "application/json", jsonResponse);
   */
  String json_str;                         //= JSON.stringify(chat_doc);
  serializeJsonPretty(chat_doc, json_str); // 文字列をシリアルポートに出力する
  Serial.println(json_str);
  server.send(200, "text/html", String(HEAD) + String("<body>") + json_str + String("</body>"));
  //  server.send(200, "text/plain", String("OK"));
}

void handle_face()
{
  String expression = server.arg("expression");
  expression = expression + "\n";
  Serial.println(expression);
  switch (expression.toInt())
  {
  case 0:
    avatar.setExpression(Expression::Neutral);
    break;
  case 1:
    avatar.setExpression(Expression::Happy);
    break;
  case 2:
    avatar.setExpression(Expression::Sleepy);
    break;
  case 3:
    avatar.setExpression(Expression::Doubt);
    break;
  case 4:
    avatar.setExpression(Expression::Sad);
    break;
  case 5:
    avatar.setExpression(Expression::Angry);
    break;
  }
  server.send(200, "text/plain", String("OK"));
}

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;
static AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);
AudioGeneratorMP3 *mp3;
AudioFileSourceVoiceTextStream *file = nullptr;
AudioFileSourceBuffer *buff = nullptr;
const int preallocateBufferSize = 50 * 1024;
uint8_t *preallocateBuffer;

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

#ifdef USE_SERVO
#define START_DEGREE_VALUE_X 90
// #define START_DEGREE_VALUE_Y 90
#define START_DEGREE_VALUE_Y 85 //
ServoEasing servo_x;
ServoEasing servo_y;
#endif

void lipSync(void *args)
{
  float gazeX, gazeY;
  int level = 0;
  DriveContext *ctx = (DriveContext *)args;
  Avatar *avatar = ctx->getAvatar();
  for (;;)
  {
    level = abs(*out.getBuffer());
    if (level < 100)
      level = 0;
    if (level > 15000)
    {
      level = 15000;
    }
    float open = (float)level / 15000.0;
    avatar->setMouthOpenRatio(open);
    avatar->getGaze(&gazeY, &gazeX);
    avatar->setRotation(gazeX * 5);
    delay(50);
  }
}

bool servo_home = false;

void servo(void *args)
{
  float gazeX, gazeY;
  DriveContext *ctx = (DriveContext *)args;
  Avatar *avatar = ctx->getAvatar();
  for (;;)
  {
#ifdef USE_SERVO
    if (!servo_home)
    {
      avatar->getGaze(&gazeY, &gazeX);
      servo_x.setEaseTo(START_DEGREE_VALUE_X + (int)(15.0 * gazeX));
      if (gazeY < 0)
      {
        int tmp = (int)(10.0 * gazeY);
        if (tmp > 10)
          tmp = 10;
        servo_y.setEaseTo(START_DEGREE_VALUE_Y + tmp);
      }
      else
      {
        servo_y.setEaseTo(START_DEGREE_VALUE_Y + (int)(10.0 * gazeY));
      }
    }
    else
    {
      //     avatar->setRotation(gazeX * 5);
      //     float b = avatar->getBreath();
      servo_x.setEaseTo(START_DEGREE_VALUE_X);
      //     servo_y.setEaseTo(START_DEGREE_VALUE_Y + b * 5);
      servo_y.setEaseTo(START_DEGREE_VALUE_Y);
    }
    synchronizeAllServosStartAndWaitForAllServosToStop();
#endif
    delay(50);
  }
}

void Servo_setup()
{
#ifdef USE_SERVO
  if (servo_x.attach(SERVO_PIN_X, START_DEGREE_VALUE_X, DEFAULT_MICROSECONDS_FOR_0_DEGREE, DEFAULT_MICROSECONDS_FOR_180_DEGREE))
  {
    Serial.print("Error attaching servo x");
  }
  if (servo_y.attach(SERVO_PIN_Y, START_DEGREE_VALUE_Y, DEFAULT_MICROSECONDS_FOR_0_DEGREE, DEFAULT_MICROSECONDS_FOR_180_DEGREE))
  {
    Serial.print("Error attaching servo y");
  }
  servo_x.setEasingType(EASE_QUADRATIC_IN_OUT);
  servo_y.setEasingType(EASE_QUADRATIC_IN_OUT);
  setSpeedForAllServos(30);
#endif
}

// char *text1 = "私の名前はスタックチャンです、よろしくね。";
// char *text2 = "こんにちは、世界！";
// char *tts_parms1 ="&emotion_level=2&emotion=happiness&format=mp3&speaker=hikari&volume=200&speed=120&pitch=130";
// char *tts_parms2 ="&emotion_level=2&emotion=happiness&format=mp3&speaker=takeru&volume=200&speed=100&pitch=130";
// char *tts_parms3 ="&emotion_level=4&emotion=anger&format=mp3&speaker=bear&volume=200&speed=120&pitch=100";
void VoiceText_tts(const char *text, const char *tts_parms)
{
  file = new AudioFileSourceVoiceTextStream(text, tts_parms);
  buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
  mp3->begin(buff, &out);
}

struct box_t
{
  int x;
  int y;
  int w;
  int h;
  int touch_id = -1;

  void setupBox(int x, int y, int w, int h)
  {
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
  }
  bool contain(int x, int y)
  {
    return this->x <= x && x < (this->x + this->w) && this->y <= y && y < (this->y + this->h);
  }
};
static box_t box_servo;

void Wifi_setup()
{
  // 前回接続時情報で接続する
  while (WiFi.status() != WL_CONNECTED)
  {
    M5.Display.print(".");
    Serial.print(".");
    delay(500);
    // 10秒以上接続できなかったら抜ける
    if (10000 < millis())
    {
      break;
    }
  }
  M5.Display.println("");
  Serial.println("");
  // 未接続の場合にはSmartConfig待受
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    WiFi.beginSmartConfig();
    M5.Display.println("Waiting for SmartConfig");
    Serial.println("Waiting for SmartConfig");
    while (!WiFi.smartConfigDone())
    {
      delay(500);
      M5.Display.print("#");
      Serial.print("#");
      // 30秒以上接続できなかったら抜ける
      if (30000 < millis())
      {
        Serial.println("");
        Serial.println("Reset");
        ESP.restart();
      }
    }
    // Wi-fi接続
    M5.Display.println("");
    Serial.println("");
    M5.Display.println("Waiting for WiFi");
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      M5.Display.print(".");
      Serial.print(".");
      // 60秒以上接続できなかったら抜ける
      if (60000 < millis())
      {
        Serial.println("");
        Serial.println("Reset");
        ESP.restart();
      }
    }
    // M5.Display.println("");
    // Serial.println("");
    // M5.Display.println("WiFi Connected.");
    // Serial.println("WiFi Connected.");
  }
  // M5.Display.print("IP Address: ");
  // Serial.print("IP Address: ");
  // M5.Display.println(WiFi.localIP());
  // Serial.println(WiFi.localIP());
}

// void info_spiffs(){
//   FSInfo fs_info;
//   SPIFFS.info(fs_info);
//   Serial.print("SPIFFS Total bytes: ");
//   Serial.println(fs_info.totalBytes);
//   Serial.print("SPIFFS Used bytes: ");
//   Serial.println(fs_info.usedBytes);
//   Serial.print("SPIFFS Free bytes: ");
//   Serial.println(fs_info.totalBytes - fs_info.usedBytes);
// }

// 3分タイマー用グローバル変数の宣言
unsigned long countdownStartMillis = 0;
int elapsedMinutes = 0;
int elapsedSeconds = 0;
bool countdownStarted = false;
bool countdownInProgress = false; // Aボタンが押されているかの判定

const char *text3T = "スタックチャンが3分間、カウントダウンをしますね。";
const char *text3T3 = "3分経ちました、カウントダウンを終了しますね。";
const char *text3TEND = "ボタンが押されたので、カウントダウンを終了しますね。";

void setup()
{
  M5.begin();
  pixels.begin();
  pixels.clear();

  // pixels.begin();
  //
  //  // LEDの初期化と消灯
  //  for (int i = 0; i < NUM_LEDS; i++)
  //  {
  //    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  //  }
  //  pixels.show();

  auto cfg = M5.config();

  cfg.external_spk = true; /// use external speaker (SPK HAT / ATOMIC SPK)
                           // cfg.external_spk_detail.omit_atomic_spk = true; // exclude ATOMIC SPK
  // cfg.external_spk_detail.omit_spk_hat    = true; // exclude SPK HAT

  M5.begin(cfg);

  preallocateBuffer = (uint8_t *)malloc(preallocateBufferSize);
  if (!preallocateBuffer)
  {
    M5.Display.printf("FATAL ERROR:  Unable to preallocate %d bytes for app\n", preallocateBufferSize);
    for (;;)
    {
      delay(1000);
    }
  }

  { /// custom setting
    auto spk_cfg = M5.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    M5.Speaker.config(spk_cfg);
  }
  M5.Speaker.begin();

  M5.Lcd.setTextSize(2);
  Serial.println("Connecting to WiFi");
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
#ifndef USE_SDCARD
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  OPENAI_API_KEY = String(OPENAI_APIKEY);
  tts_user = String(VOICETEXT_APIKEY);
#else
  /// settings
  if (SD.begin(GPIO_NUM_4, SPI, 25000000))
  {
    /// wifi
    auto fs = SD.open("/wifi.txt", FILE_READ);
    if (fs)
    {
      size_t sz = fs.size();
      char buf[sz + 1];
      fs.read((uint8_t *)buf, sz);
      buf[sz] = 0;
      fs.close();

      int y = 0;
      for (int x = 0; x < sz; x++)
      {
        if (buf[x] == 0x0a || buf[x] == 0x0d)
          buf[x] = 0;
        else if (!y && x > 0 && !buf[x - 1] && buf[x])
          y = x;
      }
      WiFi.begin(buf, &buf[y]);
    }
    else
    {
      WiFi.begin();
    }

    uint32_t nvs_handle;
    if (ESP_OK == nvs_open("apikey", NVS_READWRITE, &nvs_handle))
    {
      /// radiko-premium
      fs = SD.open("/apikey.txt", FILE_READ);
      if (fs)
      {
        size_t sz = fs.size();
        char buf[sz + 1];
        fs.read((uint8_t *)buf, sz);
        buf[sz] = 0;
        fs.close();

        int y = 0;
        for (int x = 0; x < sz; x++)
        {
          if (buf[x] == 0x0a || buf[x] == 0x0d)
            buf[x] = 0;
          else if (!y && x > 0 && !buf[x - 1] && buf[x])
            y = x;
        }

        nvs_set_str(nvs_handle, "openai", buf);
        nvs_set_str(nvs_handle, "voicetext", &buf[y]);
        Serial.println(buf);
        Serial.println(&buf[y]);
      }

      nvs_close(nvs_handle);
    }
    SD.end();
  }
  else
  {
    WiFi.begin();
  }

  {
    uint32_t nvs_handle;
    if (ESP_OK == nvs_open("apikey", NVS_READONLY, &nvs_handle))
    {
      Serial.println("nvs_open");

      size_t length1;
      size_t length2;
      if (ESP_OK == nvs_get_str(nvs_handle, "openai", nullptr, &length1) && ESP_OK == nvs_get_str(nvs_handle, "voicetext", nullptr, &length2) && length1 && length2)
      {
        Serial.println("nvs_get_str");
        char openai_apikey[length1 + 1];
        char voicetext_apikey[length2 + 1];
        if (ESP_OK == nvs_get_str(nvs_handle, "openai", openai_apikey, &length1) && ESP_OK == nvs_get_str(nvs_handle, "voicetext", voicetext_apikey, &length2))
        {
          OPENAI_API_KEY = String(openai_apikey);
          tts_user = String(voicetext_apikey);
          Serial.println(OPENAI_API_KEY);
          Serial.println(tts_user);
        }
      }
      nvs_close(nvs_handle);
    }
  }

#endif
  M5.Lcd.print("Connecting");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(250);
  //   Serial.print(".");
  //   M5.Lcd.print(".");
  // }
  Wifi_setup();
  M5.Lcd.println("\nConnected");
  Serial.printf_P(PSTR("Go to http://"));
  M5.Lcd.print("Go to http://");
  Serial.print(WiFi.localIP());
  M5.Lcd.println(WiFi.localIP());

  if (MDNS.begin("m5stack"))
  {
    Serial.println("MDNS responder started");
    M5.Lcd.println("MDNS responder started");
  }
  delay(1000);
  server.on("/", handleRoot);

  server.on("/inline", []()
            { server.send(200, "text/plain", "this works as well"); });

  // And as regular external functions:
  server.on("/speech", handle_speech);
  server.on("/face", handle_face);
  server.on("/chat", handle_chat);
  server.on("/apikey", handle_apikey);
  server.on("/apikey_set", HTTP_POST, handle_apikey_set);
  server.on("/role", handle_role);
  server.on("/role_set", HTTP_POST, handle_role_set);
  server.on("/role_get", handle_role_get);
  server.onNotFound(handleNotFound);

  init_chat_doc(json_ChatString.c_str());
  // SPIFFSをマウントする
  if (SPIFFS.begin(true))
  {
    // JSONファイルを開く
    File file = SPIFFS.open("/data.json", "r");
    if (file)
    {
      DeserializationError error = deserializeJson(chat_doc, file);
      if (error)
      {
        Serial.println("Failed to deserialize JSON");
        //      init_chat_doc(json_ChatString.c_str());
      }
      String json_str;
      serializeJsonPretty(chat_doc, json_str); // 文字列をシリアルポートに出力する
      Serial.println(json_str);
      //      info_spiffs();
    }
    else
    {
      Serial.println("Failed to open file for reading");
    }
  }
  else
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
  }

  server.begin();
  Serial.println("HTTP server started");
  M5.Lcd.println("HTTP server started");

  Serial.printf_P(PSTR("/ to control the chatGpt Server.\n"));
  M5.Lcd.print("/ to control the chatGpt Server.\n");
  delay(3000);

  audioLogger = &Serial;
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");

  Servo_setup();

  avatar.init();
  avatar.addTask(lipSync, "lipSync");
  avatar.addTask(servo, "servo");
  avatar.setSpeechFont(&fonts::efontJA_16);

  M5.Speaker.setVolume(250);
  box_servo.setupBox(80, 120, 80, 80);
}

void loop()

{
  static int lastms = 0;

  // if (Serial.available()) {
  //   char kstr[256];
  //   size_t len = Serial.readBytesUntil('\r', kstr, 256);
  //   kstr[len]=0;
  //   avatar.setExpression(Expression::Happy);
  //   VoiceText_tts(kstr, tts_parms2);
  //   avatar.setExpression(Expression::Neutral);
  // }

  M5.update();
#if defined(ARDUINO_M5STACK_Core2)
  auto count = M5.Touch.getCount();
  if (count)
  {
    auto t = M5.Touch.getDetail();
    if (t.wasPressed())
    {
#ifdef USE_SERVO
      if (box_servo.contain(t.x, t.y))
      {
        servo_home = !servo_home;
        M5.Speaker.tone(1000, 100);
      }
#endif
    }
  }
#endif

  if (M5.BtnC.wasPressed())
  {
    M5.Speaker.tone(1000, 100);
    avatar.setExpression(Expression::Happy);
    VoiceText_tts(text1, tts_parms2);
    avatar.setExpression(Expression::Neutral);
    Serial.println("mp3 begin");
  }

  // BtnAが押されたときの処理
  if (M5.BtnA.wasPressed())
  {
    if (countdownStarted)
    {
      countdownStarted = false;
      elapsedMinutes = 0;
      elapsedSeconds = 0;
      for (int i = 0; i < NUM_LEDS; i++)
      {

        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      }
      pixels.show();

      pixels.setPixelColor(2, pixels.Color(255, 0, 0));
      pixels.setPixelColor(7, pixels.Color(255, 0, 0));

      M5.Speaker.tone(1000, 100);
      VoiceText_tts(text3TEND, tts_parms2);
      pixels.show();
      delay(2000); // 2秒待機

      // 全てのLEDを消灯
      for (int i = 0; i < NUM_LEDS; i++)
      {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      }
      pixels.show();
      delay(500); // 0.5秒待機
    }
    else
    {

      for (int i = 0; i < NUM_LEDS; i++)
      {

        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      }
      pixels.show();

      pixels.setPixelColor(2, pixels.Color(0, 0, 255));
      pixels.setPixelColor(7, pixels.Color(0, 0, 255));

      M5.Speaker.tone(1000, 100);
      VoiceText_tts(text3T, tts_parms2);
      pixels.show();
      delay(3000); // 3秒待機

      countdownStarted = true;
      countdownStartMillis = millis();
    }
  }

  if (countdownStarted)
  {
    unsigned long elapsedTime = millis() - countdownStartMillis;
    int currentElapsedSeconds = elapsedTime / 500;
    if (currentElapsedSeconds != elapsedSeconds)
    {
      elapsedSeconds = currentElapsedSeconds;

      // 0.5秒ごとにLEDを更新する処理を追加
      int phase = (elapsedSeconds / 5) % 2; // 往復の方向を決定
      int pos = elapsedSeconds % 5;
      int ledIndex1, ledIndex2;

      if (phase == 0)
      { // 前進
        ledIndex1 = pos;
        ledIndex2 = NUM_LEDS - 1 - pos;
      }
      else
      { // 後退
        ledIndex1 = 4 - pos;
        ledIndex2 = 5 + pos;
      }

      pixels.clear();                             // すべてのLEDを消す
      pixels.setPixelColor(ledIndex1, 0, 0, 255); // 現在のLEDを青色で点灯
      pixels.setPixelColor(ledIndex2, 0, 0, 255); // 現在のLEDを青色で点灯
      pixels.show();                              // LEDの状態を更新

      //      // 1秒ごとにLEDを更新する処理を追加
      //      int ledIndex = elapsedSeconds % 10;
      //      int previousLedIndex = (ledIndex + 9) % 10;      // 1つ前のLEDのインデックスを計算
      //      pixels.setPixelColor(previousLedIndex, 0, 0, 0); // 1つ前のLEDを消す
      //      // pixels.setPixelColor(ledIndex - 1, 0, 0, 0); // 1つ前のLEDを消す
      //      pixels.setPixelColor(ledIndex, 0, 0, 255); // 現在のLEDを青色で点灯
      //      pixels.show();                             // LEDの状態を更新

      // 10秒間隔で読み上げ
      if (elapsedSeconds % 10 == 0 && elapsedSeconds < 180)
      {
        char buffer[64];
        if (elapsedSeconds < 60)
        {
          sprintf(buffer, "%d秒。", elapsedSeconds);
        }
        else
        {
          int minutes = elapsedSeconds / 60;
          int seconds = elapsedSeconds % 60;
          if (seconds != 0)
          {
            sprintf(buffer, "%d分%d秒。", minutes, seconds);
          }
          else
          {
            sprintf(buffer, "%d分経過。", minutes);
          }
        }
        avatar.setExpression(Expression::Happy);
        VoiceText_tts(buffer, tts_parms6);
        avatar.setExpression(Expression::Neutral);
      }
    }

    // 3分経過時にテキストを音声再生
    if (elapsedSeconds == 180)
    {
      // 全てのLEDを消す処理を追加
      for (int i = 0; i < NUM_LEDS; i++)
      {
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      }
      pixels.show();

      pixels.setPixelColor(2, pixels.Color(0, 255, 0));
      pixels.setPixelColor(7, pixels.Color(0, 255, 0));
      pixels.show();

      avatar.setExpression(Expression::Happy);
      VoiceText_tts(text3T3, tts_parms2);
      avatar.setExpression(Expression::Neutral);

      // 全てのLEDを消す処理を追加
      for (int i = 0; i < NUM_LEDS; i++)
      {
        pixels.setPixelColor(i, 0, 0, 0);
      }
      pixels.show(); // LEDの状態を更新

      // カウントダウンをリセット
      countdownStarted = false;
      elapsedMinutes = 0;
      elapsedSeconds = 0;
    }
  }

  if (speech_text != "")
  {
    speech_text_buffer = speech_text;
    speech_text = "";
    String sentence = speech_text_buffer;
    int dotIndex = speech_text_buffer.indexOf("。");
    if (dotIndex != -1)
    {
      dotIndex += 3;
      sentence = speech_text_buffer.substring(0, dotIndex);
      Serial.println(sentence);
      speech_text_buffer = speech_text_buffer.substring(dotIndex);
    }
    else
    {
      speech_text_buffer = "";
    }
    avatar.setExpression(Expression::Happy);
    VoiceText_tts((const char *)sentence.c_str(), tts_parms_table[tts_parms_no]);
    avatar.setExpression(Expression::Neutral);
  }

  if (mp3->isRunning())
  {
    if (millis() - lastms > 1000)
    {
      lastms = millis();
      Serial.printf("Running for %d ms...\n", lastms);
      Serial.flush();
    }
    if (!mp3->loop())
    {
      mp3->stop();
      if (file != nullptr)
      {
        delete file;
        file = nullptr;
      }
      Serial.println("mp3 stop");
      avatar.setExpression(Expression::Neutral);
      if (speech_text_buffer != "")
      {
        String sentence = speech_text_buffer;
        int dotIndex = speech_text_buffer.indexOf("。");
        if (dotIndex != -1)
        {
          dotIndex += 3;
          sentence = speech_text_buffer.substring(0, dotIndex);
          Serial.println(sentence);
          speech_text_buffer = speech_text_buffer.substring(dotIndex);
        }
        else
        {
          speech_text_buffer = "";
        }
        avatar.setExpression(Expression::Happy);
        VoiceText_tts((const char *)sentence.c_str(), tts_parms_table[tts_parms_no]);
        avatar.setExpression(Expression::Neutral);
      }
    }
  }
  else
  {
    server.handleClient();
  }
  // delay(100);
}
