// ===== Little Tuts! - Poster Piano Interaktif (ESP32) =====
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>

// ---------- Pin ----------
// Sesuai Tabel 1: Do, Re, Mi, Fa, Sol, La, Si, Do'
const uint8_t BTN_PIN[8] = {13, 14, 27, 26, 25, 33, 32, 18};
const char*   NOTE_NAME[8] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do'"};

const uint8_t LED_GREEN = 4;
const uint8_t LED_RED   = 15;

// DFPlayer TX ke ESP32 RX (16), DFPlayer RX dari ESP32 TX (17)
const uint8_t DF_RX = 16;
const uint8_t DF_TX = 17;

// ---------- File MP3 di SD card ----------
// Namai file di SD Card dengan 4 digit:
// 0001 - 0008.mp3 = Do sampai Do' tinggi
// 0009.mp3        = Suara kemenangan (tamat)
// 0010.mp3        = Suara gagal
// 0011.mp3        = Chord A Minor
// 0012.mp3        = Chord G Major
// 0013.mp3        = Chord F Major
// 0014.mp3        = Chord E Minor
// 0015.mp3        = Chord D Minor
// 0016.mp3        = Chord C Major
const uint8_t SND_WIN  = 9;
const uint8_t SND_FAIL = 10;

// ---------- Pengaturan level & poin ----------
const int JUMLAH_SOAL_BEGINNER     = 5;   // maksimal 8
const int JUMLAH_SOAL_INTERMEDIATE = 5;   // maksimal 8
const int POIN_BEGINNER     = 10;
const int POIN_INTERMEDIATE = 20;
const int POIN_CHORD_BENAR  = 50;
const int POIN_SALAH        = 10;
const int POIN_CHORD_SALAH  = 10;
const int BATAS_LULUS_PERSEN = 50;        // level dianggap lulus kalau jawaban benar >= persen ini

// ---------- Pengaturan waktu (milidetik, 1000 = 1 detik) ----------
const unsigned long WAKTU_DFPLAYER_BOOT  = 1500;
const unsigned long WAKTU_AUDIO_GAGAL    = 1000;
const unsigned long WAKTU_SELAMAT_DATANG = 3000;
const unsigned long SCROLL_MS            = 300;
const unsigned long WAKTU_SIAP           = 1500;
const unsigned long WAKTU_LEVEL          = 2500;
const unsigned long WAKTU_HASIL_LEVEL    = 3000;   // lama layar hasil di akhir tiap level
const unsigned long WAKTU_INFO_BEBAS     = 2000;   // lama layar sapaan & petunjuk keluar mode bebas
const unsigned long WAKTU_KELUAR_BEBAS   = 1500;   // tahan Do + Do' selama ini untuk keluar mode bebas
const unsigned long WAKTU_DIAM_BEBAS     = 60000;  // tidak ada tuts ditekan selama ini -> otomatis keluar
const unsigned long JEDA_NADA            = 2000;
const unsigned long JEDA_CHORD           = 1000;
const unsigned long WAKTU_SKOR_AKHIR     = 15000;
const unsigned long DEBOUNCE_MS          = 30;
const unsigned long CHORD_HOLD_MS        = 200;  // 3 jari harus stabil segini lama baru dinilai

struct Chord {
  const char* nama;
  const char* teks;
  uint8_t     mask;
  uint8_t     file;
};

const Chord CHORDS[] = {
  {"A Minor", "Tekan:Do+Mi+La",   (1<<0)|(1<<2)|(1<<5), 11},
  {"G Major", "Tekan:Re+Sol+Si",  (1<<1)|(1<<4)|(1<<6), 12},
  {"F Major", "Tekan:Fa+La+Do'",  (1<<3)|(1<<5)|(1<<7), 13},
  {"E Minor", "Tekan:Mi+Sol+Si",  (1<<2)|(1<<4)|(1<<6), 14},
  {"D Minor", "Tekan:Re+Fa+La",   (1<<1)|(1<<3)|(1<<5), 15},
  {"C Major", "Tekan:Do+Mi+Sol",  (1<<0)|(1<<2)|(1<<4), 16},
};
const int JUMLAH_CHORD = sizeof(CHORDS) / sizeof(CHORDS[0]);

LiquidCrystal_I2C lcd(0x27, 16, 2);
DFRobotDFPlayerMini dfp;
bool dfReady = false;
int skor = 0;
int lastNada = -1;   // nada terakhir yang ditanyakan (biar tidak sama antar level)

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

int indexDariMask(uint8_t m) {
  for (int i = 0; i < 8; i++) if (m & (1 << i)) return i;
  return -1;
}

void acak(int* arr, int n) {              // kocok array (Fisher-Yates)
  for (int i = n - 1; i > 0; i--) {
    int j = random(0, i + 1);
    int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
  }
}

void tampil(const String& baris1, const String& baris2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(baris1.substring(0, 16));
  lcd.setCursor(0, 1); lcd.print(baris2.substring(0, 16));
}

// Cetak ke satu baris LCD tanpa lcd.clear() (tidak berkedip), sisa kolom diisi spasi
void cetakBaris(uint8_t baris, const char* teks) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", teks);
  lcd.setCursor(0, baris);
  lcd.print(buf);
}

void putarSuara(uint8_t file) {
  if (dfReady) dfp.play(file);
}

void ledOff() { digitalWrite(LED_GREEN, LOW); digitalWrite(LED_RED, LOW); }

void tungguLepas() {
  while (bacaMask() != 0) delay(10);
  delay(DEBOUNCE_MS);
}

uint8_t tungguTekan() {
  while (true) {
    uint8_t m1 = bacaMask();
    if (m1) {
      delay(DEBOUNCE_MS);
      if (bacaMask()) {
        delay(40);
        return m1 | bacaMask();
      }
    }
    delay(2);
  }
}

// ---------- TAHAP 1: Standby ----------
void tahapStandby() {
  ledOff();
  const String pesan = "                Tekan tuts mana saja jika kamu sudah siap bermain...                ";
  tampil("  Little Tuts!");
  int pos = 0;
  unsigned long terakhir = 0;
  while (true) {
    if (bacaMask()) {
      delay(DEBOUNCE_MS);
      if (bacaMask()) break;
    }
    if (millis() - terakhir > SCROLL_MS) {
      terakhir = millis();
      lcd.setCursor(0, 1);
      lcd.print(pesan.substring(pos, pos + 16));
      pos = (pos + 1) % (pesan.length() - 16);
    }
  }
  tungguLepas();
}

// ---------- TAHAP 2: Menu utama ----------
// Return 1 = Game Level, 2 = Main Bebas. Hanya Tuts 1 atau Tuts 2 (satu jari) yang dihitung.
int tahapMenu() {
  ledOff();
  tampil("Tuts1:Game Level", "Tuts2:Main Bebas");
  while (true) {
    uint8_t m = tungguTekan();
    tungguLepas();
    if (m == (1 << 0)) return 1;
    if (m == (1 << 1)) return 2;
    // tuts lain / lebih dari satu jari -> abaikan, tetap tunggu pilihan
  }
}

// ---------- MODE 2: Main Bebas ----------
void modeBebas() {
  const uint8_t MASK_KELUAR = (1 << 0) | (1 << 7);   // Do + Do'
  ledOff();
  tampil("Selamat bermain", "pianis!");
  delay(WAKTU_INFO_BEBAS);
  tampil("Keluar: tahan", "Do + Do' 1,5 dtk");
  delay(WAKTU_INFO_BEBAS);
  tampil("Selamat bermain", "pianis!");
  Serial.println("Mode bebas dimulai");

  uint8_t prev = 0;
  unsigned long tKeluar = 0;                 // 0 = kombinasi keluar belum ditahan
  unsigned long tAktif = millis();           // terakhir kali ada tuts ditekan

  while (true) {
    uint8_t m = bacaMask();

    // Tuts baru ditekan (belum tertekan di putaran sebelumnya)
    uint8_t baru = m & ~prev;
    if (baru) {
      delay(DEBOUNCE_MS);
      m = bacaMask();
      baru = m & ~prev;
    }
    if (baru) {
      int idx = indexDariMask(baru);         // kalau beberapa tuts baru bersamaan, ambil yang paling rendah
      putarSuara(idx + 1);                   // 0001.mp3 - 0008.mp3
      digitalWrite(LED_GREEN, HIGH);
      char b2[20];
      snprintf(b2, sizeof(b2), "Nada: %s", NOTE_NAME[idx]);
      cetakBaris(1, b2);
      Serial.printf("Bebas: %s\n", NOTE_NAME[idx]);
    }
    if (m == 0 && prev != 0) {               // semua jari dilepas
      ledOff();
      cetakBaris(1, "pianis!");
    }
    if (m) tAktif = millis();
    prev = m;

    // Keluar: Do + Do' ditahan WAKTU_KELUAR_BEBAS
    if ((m & MASK_KELUAR) == MASK_KELUAR) {
      if (tKeluar == 0) tKeluar = millis();
      if (millis() - tKeluar >= WAKTU_KELUAR_BEBAS) break;
    } else {
      tKeluar = 0;
    }

    // Keluar otomatis kalau lama tidak dimainkan
    if (millis() - tAktif >= WAKTU_DIAM_BEBAS) break;

    delay(2);
  }

  ledOff();
  tungguLepas();
  Serial.println("Mode bebas selesai");
}

// ---------- MODE 1: Game Level ----------
void modeGame() {
  tampil("Siap-siap ya...");
  delay(WAKTU_SIAP);
  levelBeginner();
  levelIntermediate();
  levelAdvance();
  tahapAkhir();
}

// ---------- Layar pengantar level ----------
void layarLevel(const char* baris1, const char* baris2) {
  ledOff();
  tampil(baris1, baris2);
  delay(WAKTU_LEVEL);
}

// ---------- Tebak nada (Level 1 dan Level 2) ----------
int tebakNada(int jumlahSoal, int poinBenar, bool tampilkanTuts) {   // return: jumlah jawaban benar
  if (jumlahSoal > 8) jumlahSoal = 8;     // maksimal 8 biar tidak ada yang ngulang
  int jumlahBenar = 0;

  int urutan[8];
  for (int i = 0; i < 8; i++) urutan[i] = i;
  acak(urutan, 8);
  if (urutan[0] == lastNada) {            // hindari sama dengan soal terakhir level sebelumnya
    int t = urutan[0]; urutan[0] = urutan[1]; urutan[1] = t;
  }

  for (int soal = 1; soal <= jumlahSoal; soal++) {
    int target = urutan[soal - 1];
    lastNada = target;
    ledOff();
    char info[20];
    if (tampilkanTuts) snprintf(info, sizeof(info), "Tuts ke-%d   %d/%d", target + 1, soal, jumlahSoal);
    else               snprintf(info, sizeof(info), "%d/%d Skor:%d", soal, jumlahSoal, skor);
    tampil(String("Tebak Nada: ") + NOTE_NAME[target], String(info));

    uint8_t m = tungguTekan();
    int jawab = (jumlahBit(m) == 1) ? indexDariMask(m) : -1;

    if (jawab == target) {
      digitalWrite(LED_GREEN, HIGH);
      skor += poinBenar;
      jumlahBenar++;
      tampil("BENAR! +" + String(poinBenar), "Skor: " + String(skor));
    } else {
      digitalWrite(LED_RED, HIGH);
      skor -= POIN_SALAH;
      if (skor < 0) skor = 0;
      tampil("SALAH! -" + String(POIN_SALAH), "Skor: " + String(skor));
    }
    putarSuara(target + 1);               // benar/salah: tetap bunyikan nada soal (0001-0008.mp3)
    tungguLepas();
    delay(JEDA_NADA);
    ledOff();
  }
  return jumlahBenar;
}

// ---------- Layar + suara di akhir tiap level ----------
// Lulus -> SND_WIN + LED hijau, tidak lulus -> SND_FAIL + LED merah
void akhirLevel(const char* namaLevel, int jumlahBenar, int jumlahSoal) {
  bool lulus = (jumlahBenar * 100 >= jumlahSoal * BATAS_LULUS_PERSEN);
  ledOff();
  digitalWrite(lulus ? LED_GREEN : LED_RED, HIGH);
  char b1[20];
  snprintf(b1, sizeof(b1), "%s %s", namaLevel, lulus ? "Lulus!" : "Gagal");
  char b2[20];
  snprintf(b2, sizeof(b2), "Benar %d/%d S:%d", jumlahBenar, jumlahSoal, skor);
  tampil(b1, b2);
  putarSuara(lulus ? SND_WIN : SND_FAIL);
  Serial.printf("%s: benar %d/%d -> %s\n", namaLevel, jumlahBenar, jumlahSoal, lulus ? "LULUS" : "GAGAL");
  delay(WAKTU_HASIL_LEVEL);
  ledOff();
}

// ---------- LEVEL 1: Beginner ----------
void levelBeginner() {
  layarLevel("Level 1", "Beginner");
  int benar = tebakNada(JUMLAH_SOAL_BEGINNER, POIN_BEGINNER, true);
  akhirLevel("Level 1", benar, JUMLAH_SOAL_BEGINNER);
}

// ---------- LEVEL 2: Intermediate ----------
void levelIntermediate() {
  layarLevel("Level 2", "Intermediate");
  int benar = tebakNada(JUMLAH_SOAL_INTERMEDIATE, POIN_INTERMEDIATE, false);
  akhirLevel("Level 2", benar, JUMLAH_SOAL_INTERMEDIATE);
}

// ---------- Cek chord (Level 3) ----------
// Baris 1: nama chord + "Jari n/3", baris 2: petunjuk atau peringatan
void tampilStatusChord(const Chord& c, int n) {
  char b1[20];
  snprintf(b1, sizeof(b1), "%-8sJari %d/3", c.nama, n);
  cetakBaris(0, b1);
  cetakBaris(1, (n > 3) ? "Maks 3 jari!" : c.teks);
}

// Return true = benar, false = salah.
// Baru dinilai setelah TEPAT 3 jari tertekan dan stabil selama CHORD_HOLD_MS.
bool cekChord(const Chord& c) {
  tungguLepas();                       // buang jari sisa dari soal sebelumnya

  int lastN = -1;                      // paksa LCD terisi di putaran pertama
  uint8_t lastM = 0;
  unsigned long tStabil = millis();

  while (true) {
    uint8_t m = bacaMask();
    int n = jumlahBit(m);

    if (n != lastN) {                  // update LCD hanya saat jumlah jari berubah
      tampilStatusChord(c, n);
      Serial.printf("Jari=%d  mask=0x%02X\n", n, m);
      lastN = n;
    }

    if (m != lastM) {                  // kombinasi berubah -> hitung ulang waktu stabil
      lastM = m;
      tStabil = millis();
    }

    if (n == 3 && (millis() - tStabil) >= CHORD_HOLD_MS) {
      Serial.printf("Dinilai: tekan=0x%02X  target=0x%02X  -> %s\n",
                    m, c.mask, (m == c.mask) ? "BENAR" : "SALAH");
      return (m == c.mask);
    }

    delay(2);
  }
}

// ---------- LEVEL 3: Advance (Chord) ----------
void levelAdvance() {
  layarLevel("Level 3", "Advance: 3 jari!");

  int urutan[JUMLAH_CHORD];
  for (int i = 0; i < JUMLAH_CHORD; i++) urutan[i] = i;
  acak(urutan, JUMLAH_CHORD);
  int jumlahBenar = 0;

  for (int k = 0; k < JUMLAH_CHORD; k++) {
    const Chord& c = CHORDS[urutan[k]];
    ledOff();
    Serial.printf("\nSoal %d/%d: %s\n", k + 1, JUMLAH_CHORD, c.nama);

    bool benar = cekChord(c);

    if (benar) {
      digitalWrite(LED_GREEN, HIGH);
      skor += POIN_CHORD_BENAR;
      jumlahBenar++;
      tampil("BENAR! +" + String(POIN_CHORD_BENAR), "Skor: " + String(skor));
    } else {
      digitalWrite(LED_RED, HIGH);
      skor -= POIN_CHORD_SALAH;
      if (skor < 0) skor = 0;
      tampil("SALAH! -" + String(POIN_CHORD_SALAH), "Skor: " + String(skor));
    }
    putarSuara(c.file);                   // benar/salah: tetap bunyikan chord soal (0011-0016.mp3)
    tungguLepas();
    delay(JEDA_CHORD);
    ledOff();
  }
  akhirLevel("Level 3", jumlahBenar, JUMLAH_CHORD);
}

// ---------- TAHAP 4: Skor akhir ----------
void tahapAkhir() {
  ledOff();
  digitalWrite(LED_GREEN, HIGH);
  tampil("Selamat! Tamat", "Skor Akhir: " + String(skor));
  delay(WAKTU_SKOR_AKHIR);
  ledOff();
  skor = 0;
  lastNada = -1;
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
  delay(WAKTU_DFPLAYER_BOOT);
  for (int coba = 1; coba <= 3 && !dfReady; coba++) {
    dfReady = dfp.begin(Serial2, true, true);
    if (!dfReady) {
      Serial.printf("DFPlayer percobaan %d gagal\n", coba);
      delay(1000);
    }
  }
  if (dfReady) {
    dfp.volume(25);
    Serial.println("DFPlayer siap");
  } else {
    Serial.println("DFPlayer tidak terdeteksi, lanjut tanpa suara");
    tampil("Audio tidak ada", "Cek SD/kabel");
    delay(WAKTU_AUDIO_GAGAL);
  }

  tampil("Selamat Datang", "di Little Tuts!");
  delay(WAKTU_SELAMAT_DATANG);
  randomSeed(esp_random());
}

void loop() {
  tahapStandby();
  while (true) {
    int pilihan = tahapMenu();
    if (pilihan == 1) { modeGame(); break; }   // selesai game -> kembali ke standby
    modeBebas();                               // selesai main bebas -> kembali ke menu
  }
}
