#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>

const char *ssid = "Kamu Mau?";
const char *password = "adadikolongmeja";
const char *botToken = "7743898350:AAFaqTTrWEqNORw4JAQI-sOs_vj5WIQwjzM";
const char *chatID = "1327279170";

WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);
RTC_DS1307 rtc;

// PWM setup
#define pinPWM 19
const int pwmChannel = 0;
const int freq = 20000;   // 20 kHz
const int resolution = 8; // 8-bit resolution

// Data pemberian pakan
String lastFeedTime = "Belum ada";
String latestScheduledTime = "Belum diset";
bool waitingForTimeInput = false;
bool pakanSudahDiberikan = false;

// Flag pengiriman menu otomatis
bool menuPagiTerkirim = false;
bool menuSoreTerkirim = false;

// Fungsi kirim menu
void kirimMenu()
{
  String menuMsg =
      "Silakan Kirim:\n"
      "1 = Pemberian pakan sekarang\n"
      "2 = Pengaturan pemberian pakan (HH:MM)\n"
      "3 = Melihat jam makan terbaru\n"
      "4 = Pemberian pakan terakhir";
  bot.sendMessage(chatID, menuMsg, "");
}

// Fungsi pemberian pakan
void beriPakan()
{
  ledcWrite(pwmChannel, 128); // duty cycle 50%
  delay(3000);                // nyala 3 detik
  ledcWrite(pwmChannel, 0);

  DateTime now = rtc.now();
  lastFeedTime = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  bot.sendMessage(chatID, "Pakan diberikan sekarang! 🐟", "");
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();

  // PWM setup
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(pinPWM, pwmChannel);
  ledcWrite(pwmChannel, 0);

  // Koneksi WiFi
  secured_client.setInsecure();
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung");

  if (!rtc.isrunning())
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop()
{
  DateTime now = rtc.now();

  // Kirim menu otomatis jam 07:15
  if (now.hour() == 7 && now.minute() == 15)
  {
    if (!menuPagiTerkirim)
    {
      kirimMenu();
      bot.sendMessage(chatID, "Menu pagi telah dikirim (07:15)", "");
      menuPagiTerkirim = true;
    }
  }
  else
  {
    menuPagiTerkirim = false;
  }

  // Kirim menu otomatis jam 17:15
  if (now.hour() == 17 && now.minute() == 15)
  {
    if (!menuSoreTerkirim)
    {
      kirimMenu();
      bot.sendMessage(chatID, "Menu sore telah dikirim (17:15)", "");
      menuSoreTerkirim = true;
    }
  }
  else
  {
    menuSoreTerkirim = false;
  }

  // Pemberian pakan otomatis sesuai jadwal yang diatur dari Telegram
  if (latestScheduledTime != "Belum diset")
  {
    int scheduledHour = latestScheduledTime.substring(0, 2).toInt();
    int scheduledMinute = latestScheduledTime.substring(3, 5).toInt();

    if (now.hour() == scheduledHour && now.minute() == scheduledMinute)
    {
      if (!pakanSudahDiberikan)
      {
        beriPakan();
        pakanSudahDiberikan = true;
      }
    }
    else
    {
      pakanSudahDiberikan = false; // reset untuk hari berikutnya
    }
  }

  // Cek pesan masuk dari Telegram
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages)
  {
    for (int i = 0; i < numNewMessages; i++)
    {
      String text = bot.messages[i].text;
      String fromID = bot.messages[i].chat_id;

      if (fromID != chatID)
      {
        bot.sendMessage(fromID, "Maaf, Anda tidak memiliki akses!", "");
        continue;
      }

      if (waitingForTimeInput)
      {
        int separator = text.indexOf(':');
        if (separator != -1 && text.length() == 5)
        {
          String jam = text.substring(0, separator);
          String menit = text.substring(separator + 1);

          int jamInt = jam.toInt();
          int menitInt = menit.toInt();

          if (jamInt >= 0 && jamInt < 24 && menitInt >= 0 && menitInt < 60)
          {
            latestScheduledTime = text;
            bot.sendMessage(chatID, "Jadwal pemberian pakan disimpan: " + text, "");
          }
          else
          {
            bot.sendMessage(chatID, "Format waktu tidak valid. Gunakan HH:MM (contoh: 15:20)", "");
          }
        }
        else
        {
          bot.sendMessage(chatID, "Format tidak sesuai. Harap masukkan seperti 15:20", "");
        }

        waitingForTimeInput = false;
      }
      else if (text == "menu")
      {
        kirimMenu();
      }
      else if (text == "1")
      {
        beriPakan();
      }
      else if (text == "2")
      {
        bot.sendMessage(chatID, "Masukkan jam pemberian pakan dalam format HH:MM (contoh: 15:20)", "");
        waitingForTimeInput = true;
      }
      else if (text == "3")
      {
        bot.sendMessage(chatID, "Jam makan terbaru: " + latestScheduledTime, "");
      }
      else if (text == "4")
      {
        bot.sendMessage(chatID, "Pakan terakhir diberikan pada: " + lastFeedTime, "");
      }
      else
      {
        bot.sendMessage(chatID, "Perintah tidak dikenali. Ketik 'menu' untuk melihat opsi.", "");
      }
    }

    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  delay(2000); // polling delay
}
