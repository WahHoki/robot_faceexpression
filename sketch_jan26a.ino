#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <U8g2lib.h>

// ==========================================
// BAGIAN INI WAJIB ANDA ISI SEBELUM UPLOAD
// ==========================================

// 1. NAMA WIFI (SSID)
// Saya set ke "Mezaya FC" (Versi 2.4GHz). 
// Jika router rumah tidak punya nama ini, GANTI DENGAN NAMA HOTSPOT HP ANDA.
const char* ssid = "Mezaya FC";     

// 2. PASSWORD WIFI
// Tulis password wifi rumah/hotspot Anda di dalam tanda kutip di bawah ini:
const char* password = "mantapdonk"; 

// 3. IP LAPTOP (Sudah sesuai screenshot Anda)
// Pastikan laptop konek ke WiFi yang SAMA dengan ESP32
const char* pcIp = "192.168.1.74";       
const int udpPort = 4444;

// ==========================================
// KONFIGURASI HARDWARE (JANGAN DIUBAH)
// ==========================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 22, 21);
#define I2S_WS 15
#define I2S_SD 32
#define I2S_SCK 14
#define I2S_PORT I2S_NUM_0
#define BUFFER_LEN 1024

WiFiUDP udp;
int16_t sBuffer[BUFFER_LEN];
String currentEmotion = "NEUTRAL"; 

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  
  // Tampilan "Menghubungkan"
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 30, "Menghubungkan...");
  u8g2.drawStr(10, 45, ssid); // Menampilkan nama WiFi yang dicoba
  u8g2.sendBuffer();

  // Proses Koneksi WiFi
  WiFi.begin(ssid, password);
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry_count++;
    // Jika lebih dari 20 titik (10 detik) tidak konek, kemungkinan salah password/SSID
    if(retry_count > 20){
        u8g2.clearBuffer();
        u8g2.drawStr(0, 30, "Gagal Konek!");
        u8g2.drawStr(0, 45, "Cek 2.4GHz / Pass");
        u8g2.sendBuffer();
    }
  }
  
  // Jika Berhasil Konek
  Serial.println("\nWiFi Connected!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
  
  u8g2.clearBuffer();
  u8g2.drawStr(10, 30, "Terhubung!");
  u8g2.drawStr(10, 50, "Siap Hoci!");
  u8g2.sendBuffer();
  delay(1000);
  
  udp.begin(udpPort); 

  // Setup Mic I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_pin_config_t pin_config = { 
    .bck_io_num = I2S_SCK, 
    .ws_io_num = I2S_WS, 
    .data_out_num = I2S_PIN_NO_CHANGE, 
    .data_in_num = I2S_SD 
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void loop() {
  size_t bytesIn = 0;
  
  // 1. BACA SUARA (Non-Blocking)
  // Menggunakan portMAX_DELAY 0 agar loop tidak macet menunggu suara
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, BUFFER_LEN * sizeof(int16_t), &bytesIn, 0); 
  
  // 2. KIRIM KE PYTHON (Hanya jika ada data suara masuk)
  if (result == ESP_OK && bytesIn > 0) {
    udp.beginPacket(pcIp, udpPort);
    udp.write((const uint8_t*)sBuffer, bytesIn);
    udp.endPacket();
  }

  // 3. TERIMA PERINTAH DARI PYTHON
  int packetSize = udp.parsePacket();
  if (packetSize) {
    String message = udp.readString();
    message.trim(); 
    if(message.length() > 0) {
        currentEmotion = message; // Update Emosi
        Serial.println("Cmd: " + message);
    }
  }

  // 4. UPDATE LAYAR
  drawFace(currentEmotion);
}

void drawFace(String emotion) {
  u8g2.clearBuffer();
  
  if (emotion == "HAPPY") {
    // SENANG
    u8g2.drawArc(40, 35, 10, 180, 360);
    u8g2.drawArc(88, 35, 10, 180, 360);
    u8g2.drawFilledEllipse(64, 45, 15, 8);
    u8g2.setDrawColor(0); u8g2.drawBox(40, 30, 50, 15); u8g2.setDrawColor(1);
    u8g2.drawStr(35, 62, "Mendengarkan..");
    
  } else if (emotion == "SAD") {
    // SEDIH
    u8g2.drawDisc(40, 30, 3); 
    u8g2.drawDisc(88, 30, 3);
    u8g2.drawArc(64, 55, 15, 180, 360);
    u8g2.drawStr(50, 15, "Sedih..");
    
  } else if (emotion == "ANGRY") {
    // MARAH
    u8g2.drawLine(30, 25, 50, 35);
    u8g2.drawLine(98, 25, 78, 35);
    u8g2.drawCircle(40, 35, 5);
    u8g2.drawCircle(88, 35, 5);
    u8g2.drawBox(54, 50, 20, 5);
    u8g2.drawStr(45, 15, "Marah!!");
    
  } else { 
    // NEUTRAL / HENING
    u8g2.drawCircle(40, 30, 8);
    u8g2.drawCircle(88, 30, 8);
    u8g2.drawLine(50, 50, 78, 50);
    // u8g2.drawStr(40, 15, "Hoci Aktif");
  }
  
  u8g2.sendBuffer();
}