#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ---------- Konfigurasi WiFi & Telegram ----------
const char* ssid     = "Hp doang bagus";
const char* password = "tapigapunyakuota";
const char* botToken = "7743898350:AAFaqTTrWEqNORw4JAQI-sOs_vj5WIQwjzM";
const char* chatID   = "1327279170";

// ---------- Pin solenoid & LED ----------
const int motorPin     = 19;
const int ledBlink     = 15;
const int ledConnected = 4;

// ---------- Objek global ----------
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);
RTC_DS1307 rtc;

// NTP Client setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 3600000); // Offset 0, update setiap 1 jam

// ---------- Status & mode input ----------
String lastFeedTime        = "Belum ada";
String latestScheduledTime = "Belum diset";
int scheduledDuration      = 10;   // durasi default untuk jadwal (detik)
bool waitingForTimeInput   = false;
bool waitingForDuration    = false;
bool waitingForScheduleDur = false;
bool pakanSudahDiberikan   = false;
bool menuPagiTerkirim      = false;
bool menuSoreTerkirim      = false;

void kirimMenu();
void beriPakan(int durasiDetik);

void setup() {
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();
  secured_client.setInsecure();
  pinMode(motorPin, OUTPUT);
  pinMode(ledBlink, OUTPUT);
  pinMode(ledConnected, OUTPUT);
  digitalWrite(motorPin, LOW);
  digitalWrite(ledBlink, LOW);
  digitalWrite(ledConnected, LOW);

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledBlink, HIGH);
    delay(200);
    digitalWrite(ledBlink, LOW);
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  digitalWrite(ledConnected, HIGH);
  bot.sendMessage(chatID, "🤖 Pakan Ikan online.");
  timeClient.begin();
  timeClient.update();
  long epochTime = timeClient.getEpochTime();

  // Konversi ke zona waktu WIB (GMT +7)
  long adjustedTime = epochTime + (7 * 3600);
  rtc.adjust(DateTime(adjustedTime));

  // Debug waktu RTC
  DateTime now = rtc.now();
  Serial.print("RTC time set to: ");
  Serial.println(now.timestamp(DateTime::TIMESTAMP_TIME));
}

void loop() {
  DateTime now = rtc.now();

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    Serial.print("RTC: ");
    Serial.println(now.timestamp(DateTime::TIMESTAMP_TIME));
    lastPrint = millis();
  }

  // Kirim menu otomatis pagi (10:30) dan sore (17:15)
  if (now.hour() == 10 && now.minute() == 30 && !menuPagiTerkirim) { kirimMenu(); menuPagiTerkirim = true; }
  if (!(now.hour() == 10 && now.minute() == 30)) menuPagiTerkirim = false;
  if (now.hour() == 17 && now.minute() == 15 && !menuSoreTerkirim) { kirimMenu(); menuSoreTerkirim = true; }
  if (!(now.hour() == 17 && now.minute() == 15)) menuSoreTerkirim = false;

  // Pakan otomatis berdasarkan jadwal terakhir dan durasi yang di-set
  if (latestScheduledTime != "Belum diset") {
    int sep = latestScheduledTime.indexOf(':');
    int schHour = latestScheduledTime.substring(0, sep).toInt();
    int schMin  = latestScheduledTime.substring(sep + 1).toInt();
    if (now.hour() == schHour && now.minute() == schMin) {
      if (!pakanSudahDiberikan) {
        beriPakan(scheduledDuration);
        pakanSudahDiberikan = true;
      }
    } else pakanSudahDiberikan = false;
  }

  // Handle pesan Telegram
  int newMsg = bot.getUpdates(bot.last_message_received + 1);
  while (newMsg) {
    for (int i = 0; i < newMsg; i++) {
      String txt = bot.messages[i].text;
      String from = bot.messages[i].chat_id;
      if (from != chatID) { bot.sendMessage(from, "🚫 Tidak punya akses."); continue; }

      if (waitingForTimeInput) {
        waitingForTimeInput = false;
        int sep = txt.indexOf(':');
        if (sep > 0) {
          int jam = txt.substring(0, sep).toInt();
          int menit = txt.substring(sep + 1).toInt();
          if (jam>=0 && jam<24 && menit>=0 && menit<60) {
            latestScheduledTime = String(jam) + ":" + String(menit);
            bot.sendMessage(chatID, "✅ Jadwal disimpan: " + latestScheduledTime);

            // Setelah menerima input waktu, minta durasi pakan
            bot.sendMessage(chatID, "Masukkan durasi untuk jadwal pakan (detik, max 300):");
            waitingForScheduleDur = true;
          } else {
            bot.sendMessage(chatID, "Format salah. Gunakan HH:MM.");
          }
        } else {
          bot.sendMessage(chatID, "Format salah. Gunakan HH:MM.");
        }
        continue;
      }

      if (waitingForScheduleDur) {
        waitingForScheduleDur = false;
        int durasi = txt.toInt();
        if (durasi > 0 && durasi <= 300) {
          scheduledDuration = durasi;
          bot.sendMessage(chatID, "✅ Durasi jadwal disimpan: " + String(scheduledDuration) + " detik");
        } else {
          bot.sendMessage(chatID, "Jumlah detik salah. Masukkan 1–300.");
        }
        continue;
      }

      if (waitingForDuration) {
        waitingForDuration = false;
        int detik = txt.toInt();
        if (detik > 0 && detik <= 300) {
          beriPakan(detik);
        } else {
          bot.sendMessage(chatID, "Durasi salah. Masukkan 1–300.");
        }
        continue;
      }

      // Menu utama
      if (txt == "menu") {
        kirimMenu();
      } else if (txt == "1") {
        bot.sendMessage(chatID, "Masukkan durasi pakan sekarang (detik, max 300):");
        waitingForDuration = true;
      } else if (txt == "2") {
        bot.sendMessage(chatID, "Masukkan jadwal pakan (HH:MM):");
        waitingForTimeInput = true;
      } else if (txt == "3") {
        bot.sendMessage(chatID, "🕓 Jadwal: " + latestScheduledTime + "\n⏱️ Durasi: " + String(scheduledDuration) + " detik");
      } else if (txt == "4") {
        bot.sendMessage(chatID, "🕔 Pakan terakhir: " + lastFeedTime);
      } else {
        bot.sendMessage(chatID, "Perintah tidak dikenali. Ketik 'menu'.");
      }
    }
    newMsg = bot.getUpdates(bot.last_message_received + 1);
  }
  delay(250);
}

void kirimMenu() {
  bot.sendMessage(chatID,
    "Silakan pilih:\n"
    "1 = Beri pakan sekarang\n"
    "2 = Atur jadwal & durasi (HH:MM + detik)\n"
    "3 = Lihat jadwal & durasi\n"
    "4 = Lihat pakan terakhir"
  );
}

void beriPakan(int durasiDetik) {
  DateTime now = rtc.now();
  lastFeedTime = now.timestamp(DateTime::TIMESTAMP_TIME);
  bot.sendMessage(chatID, "🐟 Pakan ON selama " + String(durasiDetik) + " detik (" + lastFeedTime + ")");
  digitalWrite(motorPin, HIGH);
  delay(durasiDetik * 1000);
  digitalWrite(motorPin, LOW);
  bot.sendMessage(chatID, "🐟 Pakan OFF. Total: " + String(durasiDetik) + " detik");
  Serial.println("Pakan selesai: " + lastFeedTime);
}
