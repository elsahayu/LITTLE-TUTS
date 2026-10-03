// ===== Little Tuts! - Poster Piano Interaktif (ESP32) =====
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>

// ---------- Pin ----------
const uint8_t BTN_PIN[8] = {13, 23, 14, 27, 26, 25, 33, 32};  // Do Re Mi Fa Sol La Si Do'
const char*   NOTE_NAME[8] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do'"};
const uint8_t LED_GREEN = 4;
const uint8_t LED_RED   = 18;
const uint8_t DF_RX = 16;   // ESP32 RX2 <- DFPlayer TX
const uint8_t DF_TX = 17;   // ESP32 TX2 -> DFPlayer RX (lewat resistor 1k)

// ---------- File MP3 di SD card ----------
// 001-008 = Do..Do' | 009 = kemenangan | 010-012 = chord
const uint8_t SND_WIN = 9;

// ---------- Pengaturan game ----------
const int JUMLAH_SOAL_NADA   = 8;
const int POIN_BENAR         = 20;
const int POIN_SALAH         = 5;
const int POIN_CHORD_BENAR   = 50;
const int POIN_CHORD_SALAH   = 10;
const unsigned long DEBOUNCE_MS      = 30;
const unsigned long CHORD_WINDOW_MS  = 350;  // waktu menunggu 3 tombol ditekan "bersamaan"
const unsigned long CHORD_TIMEOUT_MS = 4000; // batas waktu menekan setelah tombol pertama

struct Chord {
  const char* nama;
  const char* teks;       // maks 16 karakter di LCD
  uint8_t     mask;       // bit i = tombol index i
  uint8_t     file;       // nomor file mp3
};
const Chord CHORDS[] = {
  {"Chord C",  "Tekan:Do+Mi+Sol", (1<<0)|(1<<2)|(1<<4), 10},
  {"Chord F",  "Tekan:Fa+La+Do'", (1<<3)|(1<<5)|(1<<7), 11},
  {"Chord Am", "Tekan:Do+Mi+La",  (1<<0)|(1<<2)|(1<<5), 12},
};
const int JUMLAH_CHORD = sizeof(CHORDS) / sizeof(CHORDS[0]);

LiquidCrystal_I2C lcd(0x27, 16, 2);
DFRobotDFPlayerMini dfp;
bool dfReady = false;
int skor = 0;

// ---------- Helper ----------
uint8_t bacaMask() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < 8; i++)
    if (digitalRead(BTN_PIN[i]) == LOW) m |= (1 << i);
  return m;
}

int jumlahBit(uint8_t m) {
  int c = 0;
  while (m) { c += m & 1; m >>= 1; }
  return c;
}

int indexDariMask(uint8_t m) {            // untuk mask 1 tombol
  for (int i = 0; i < 8; i++) if (m & (1 << i)) return i;
  return -1;
}

void tampil(const String& baris1, const String& baris2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(baris1);
  lcd.setCursor(0, 1); lcd.print(baris2);
}

void putarSuara(uint8_t file) {
  if (dfReady) dfp.play(file);
}

void ledOff() { digitalWrite(LED_GREEN, LOW); digitalWrite(LED_RED, LOW); }

void tungguLepas() {                      // tunggu semua tombol dilepas
  while (bacaMask() != 0) delay(10);
  delay(DEBOUNCE_MS);
}

// Tunggu sampai ada tombol ditekan (debounced), kembalikan mask awal
uint8_t tungguTekan() {
  while (true) {
    uint8_t m = bacaMask();
    if (m) {
      delay(DEBOUNCE_MS);
      if (bacaMask()) return bacaMask();
    }
    delay(2);
  }
}

// ---------- TAHAP 1: Standby ----------
void tahapStandby() {
  ledOff();
  const String pesan = "                Tekan tuts mana saja jika kamu sudah siap bermain...                ";
  tampil("   Little Tuts!");
  int pos = 0;
  unsigned long terakhir = 0;
  while (true) {
    if (bacaMask()) {                     // pemindaian tombol terus-menerus
      delay(DEBOUNCE_MS);
      if (bacaMask()) break;
    }
    if (millis() - terakhir > 300) {      // teks berjalan non-blocking
      terakhir = millis();
      lcd.setCursor(0, 1);
      lcd.print(pesan.substring(pos, pos + 16));
      pos = (pos + 1) % (pesan.length() - 16);
    }
  }
  tungguLepas();
  tampil("Siap-siap...");
  delay(1500);
}

// ---------- TAHAP 2: Level 1 tebak nada ----------
void level1() {
  for (int soal = 1; soal <= JUMLAH_SOAL_NADA; soal++) {
    int target = random(0, 8);
    ledOff();
    tampil(String("Tebak Nada: ") + NOTE_NAME[target],
           String("Soal ") + soal + "/" + JUMLAH_SOAL_NADA + " Skor:" + skor);

    uint8_t m = tungguTekan();
    int jawab = (jumlahBit(m) == 1) ? indexDariMask(m) : -1;

    if (jawab == target) {
      digitalWrite(LED_GREEN, HIGH);
      skor += POIN_BENAR;
      tampil("BENAR! +" + String(POIN_BENAR), "Skor: " + String(skor));
      putarSuara(target + 1);
    } else {
      digitalWrite(LED_RED, HIGH);
      skor -= POIN_SALAH;
      if (skor < 0) skor = 0;
      tampil("SALAH! -" + String(POIN_SALAH), "Skor: " + String(skor));
    }
    tungguLepas();
    delay(1200);
    ledOff();
  }
}

// ---------- TAHAP 3: Level 2 chord ----------
void level2() {
  ledOff();
  tampil("Level 2: Chord!", "Siapkan 3 jari..");
  delay(2000);

  for (int i = 0; i < JUMLAH_CHORD; i++) {
    const Chord& c = CHORDS[i];
    ledOff();
    tampil(c.nama, c.teks);

    // tunggu tombol pertama, lalu kumpulkan semua tombol dalam jendela waktu
    unsigned long mulai = millis();
    uint8_t terkumpul = 0;
    bool mulaiTekan = false;
    unsigned long tPertama = 0;
    while (true) {
      uint8_t m = bacaMask();
      if (m) {
        if (!mulaiTekan) { mulaiTekan = true; tPertama = millis(); }
        terkumpul |= m;
      }
      if (mulaiTekan && millis() - tPertama > CHORD_WINDOW_MS) break;
      delay(2);
    }

    if (terkumpul == c.mask) {
      digitalWrite(LED_GREEN, HIGH);
      skor += POIN_CHORD_BENAR;
      tampil("BENAR! +" + String(POIN_CHORD_BENAR), "Skor: " + String(skor));
      putarSuara(c.file);
    } else {
      digitalWrite(LED_RED, HIGH);
      skor -= POIN_CHORD_SALAH;
      if (skor < 0) skor = 0;
      tampil("SALAH! -" + String(POIN_CHORD_SALAH), "Skor: " + String(skor));
    }
    tungguLepas();
    delay(1800);
    ledOff();
  }
}

// ---------- TAHAP 4: Skor akhir ----------
void tahapAkhir() {
  ledOff();
  digitalWrite(LED_GREEN, HIGH);
  tampil("Selamat! Tamat", "Skor Akhir: " + String(skor));
  putarSuara(SND_WIN);
  delay(10000);                            //waktu
  ledOff();
  skor = 0;                               // reset -> loop() kembali ke Tahap 1
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 8; i++) pinMode(BTN_PIN[i], INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  ledOff();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  tampil("Little Tuts!", "Memulai...");

  Serial2.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  delay(1000);
  dfReady = dfp.begin(Serial2);
  if (dfReady) {
    dfp.volume(25);                       // 0-30
    Serial.println("DFPlayer siap");
  } else {
    Serial.println("DFPlayer tidak terdeteksi, lanjut tanpa suara");
    tampil("Audio tidak ada", "Cek SD/kabel");
    delay(1500);
  }
  randomSeed(esp_random());
}

void loop() {
  tahapStandby();
  level1();
  level2();
  tahapAkhir();
}
