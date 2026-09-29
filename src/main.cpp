#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int pinTuts[8] = {13, 12, 14, 27, 26, 25, 33, 32};
const char* namaNada[8] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do'"};
const int frekuensi[8] = {262, 294, 330, 349, 392, 440, 494, 523};

const int totalChord = 3;
const char* namaChord[totalChord] = {"Chord C", "Chord F", "Chord Am"};
const char* petunjukChord[totalChord] = {"Do + Mi + Sol", "Fa + La + Do'", "Do + Mi + La"};

const int chordNotes[totalChord][3] = {
  {0, 2, 4}, // C  -> Do(0), Mi(2), Sol(4)  -> Angka 135
  {3, 5, 7}, // F  -> Fa(3), La(5), Do'(7)  -> Angka 468
  {0, 2, 5}  // Am -> Do(0), Mi(2), La(5)   -> Angka 136
};

const int ledHijau = 4;
const int ledMerah = 2;
const int pinBuzzer = 15;

int currentLevel = 1; 
int targetNada = 0;
int targetChord = 0;
int jumlahSalah = 0; 

// Penampung input karakter serial untuk Level 2 (Chord)
String chordBuffer = "";

void tampilkanSambutan();
void tampilkanInstruksi();
void jawabanBenar();
void jawabanSalah();
void cekLevelBeginner();
void evaluasiBeginner();
void cekLevelIntermediate();
void cekInputSerialKeyboard();

void setup() {
  Serial.begin(115200);   // Membuka komunikasi Serial
  Serial.setTimeout(10);  // Timeout singkat untuk respon responsif
  
  lcd.init();
  lcd.backlight();
  
  pinMode(ledHijau, OUTPUT);
  pinMode(ledMerah, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);

  for(int i = 0; i < 8; i++) {
    pinMode(pinTuts[i], INPUT_PULLUP);
  }

  // Tampilkan sambutan awal & informasi level pertama
  tampilkanSambutan();

  // Tampilkan instruksi nada pertama
  tampilkanInstruksi();
}

void loop() {
  // Read input tombol fisik Wokwi
  if (currentLevel == 1) {
    cekLevelBeginner();
  } else if (currentLevel == 2) {
    cekLevelIntermediate();
  }

  // Read input angka keyboard dari Serial Monitor Terminal
  cekInputSerialKeyboard();
}

void tampilkanSambutan() {
  lcd.clear();
  digitalWrite(ledHijau, LOW);
  digitalWrite(ledMerah, LOW);

  // Layar 1: Menyambut Pemain
  lcd.setCursor(0, 0); lcd.print("Selamat Datang!");
  lcd.setCursor(0, 1); lcd.print("Di Smart Piano");
  delay(2000);

  // Layar 2: Pengumuman Level Pertama (Format serupa dengan Level Intermediate)
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Level Pertama:");
  lcd.setCursor(0, 1); lcd.print("Lv Beginner");
  delay(2000);
}

void cekLevelBeginner() {
  for(int i = 0; i < 8; i++) {
    if(digitalRead(pinTuts[i]) == LOW) { 
      tone(pinBuzzer, frekuensi[i], 300); 
      if(i == targetNada) {
        jawabanBenar();
        targetNada++;
        delay(800);
        if(targetNada >= 8) evaluasiBeginner();
        else tampilkanInstruksi();
      } else {
        jawabanSalah();
        jumlahSalah++;
        delay(800);
        tampilkanInstruksi();
      }
      while(digitalRead(pinTuts[i]) == LOW) { delay(10); } 
    }
  }
}

void evaluasiBeginner() {
  lcd.clear();
  digitalWrite(ledHijau, LOW);
  digitalWrite(ledMerah, LOW);

  if(jumlahSalah <= 1) {
    lcd.setCursor(0, 0); lcd.print("Kamu lulus dari");
    lcd.setCursor(0, 1); lcd.print("level beginner!");
    delay(2000);
    
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Selanjutnya:");
    lcd.setCursor(0, 1); lcd.print("Lv Intermediate");
    delay(2000);
    
    currentLevel = 2;
    targetChord = 0;
    chordBuffer = ""; // Reset buffer
    tampilkanInstruksi();
  } else {
    lcd.setCursor(0, 0); lcd.print("Anda berhasil");
    lcd.setCursor(0, 1); lcd.print("menyelesaikan");
    delay(2000);
    
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("level beginner");
    delay(2000);
    
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Sistem Selesai");
    currentLevel = 0;
  }
}

void cekLevelIntermediate() {
  int n1 = chordNotes[targetChord][0];
  int n2 = chordNotes[targetChord][1];
  int n3 = chordNotes[targetChord][2];

  bool tekan1 = (digitalRead(pinTuts[n1]) == LOW);
  bool tekan2 = (digitalRead(pinTuts[n2]) == LOW);
  bool tekan3 = (digitalRead(pinTuts[n3]) == LOW);
  
  bool tombolLainDitekan = false;
  for(int i = 0; i < 8; i++) {
    if(i != n1 && i != n2 && i != n3) {
      if(digitalRead(pinTuts[i]) == LOW) tombolLainDitekan = true;
    }
  }

  if(tekan1 && tekan2 && tekan3 && !tombolLainDitekan) {
    tone(pinBuzzer, frekuensi[n1], 150); delay(150);
    tone(pinBuzzer, frekuensi[n2], 150); delay(150);
    tone(pinBuzzer, frekuensi[n3], 150); delay(150);
    
    jawabanBenar();
    targetChord++;
    delay(1500);

    if(targetChord >= totalChord) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Selamat! Anda");
      lcd.setCursor(0, 1); lcd.print("Lulus Semua Lv!");
      currentLevel = 0;
    } else {
      tampilkanInstruksi();
    }

    while(digitalRead(pinTuts[n1]) == LOW || digitalRead(pinTuts[n2]) == LOW || digitalRead(pinTuts[n3]) == LOW) {
      delay(10);
    }
  }
}

void cekInputSerialKeyboard() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    // 1. Abaikan karakter Newline (\n), Carriage Return (\r), dan Spasi
    if (c == '\n' || c == '\r' || c == ' ') {
      continue;
    }

    // 2. Hanya proses jika input berupa angka '1' sampai '8'
    if (c >= '1' && c <= '8') {

      // --- LEVEL 1: BEGINNER (Evaluasi Instan Per Karakter) ---
      if (currentLevel == 1) {
        int i = c - '1'; // Konversi '1'-'8' ke index array 0-7

        tone(pinBuzzer, frekuensi[i], 300);
        if (i == targetNada) {
          jawabanBenar();
          targetNada++;
          delay(800);
          if (targetNada >= 8) evaluasiBeginner();
          else tampilkanInstruksi();
        } else {
          jawabanSalah();
          jumlahSalah++;
          delay(800);
          tampilkanInstruksi();
        }
      } 
      
      // --- LEVEL 2: INTERMEDIATE (Tampung 3 Karakter Baru Evaluasi) ---
      else if (currentLevel == 2) {
        chordBuffer += c; // Tambahkan angka ke penampung (misal: "1" -> "13" -> "135")

        // Evaluasi hanya setelah 3 angka terkumpul
        if (chordBuffer.length() >= 3) {
          bool serialPressed[8] = {false};

          // Tandai nada yang diketik pada array boolean
          for (int idx = 0; idx < chordBuffer.length(); idx++) {
            char ch = chordBuffer.charAt(idx);
            if (ch >= '1' && ch <= '8') {
              serialPressed[ch - '1'] = true;
            }
          }

          int n1 = chordNotes[targetChord][0];
          int n2 = chordNotes[targetChord][1];
          int n3 = chordNotes[targetChord][2];

          // Cek kelengkapan 3 nada chord
          if (serialPressed[n1] && serialPressed[n2] && serialPressed[n3]) {
            tone(pinBuzzer, frekuensi[n1], 150); delay(150);
            tone(pinBuzzer, frekuensi[n2], 150); delay(150);
            tone(pinBuzzer, frekuensi[n3], 150); delay(150);

            jawabanBenar();
            targetChord++;
            delay(1000);

            if (targetChord >= totalChord) {
              lcd.clear();
              lcd.setCursor(0, 0); lcd.print("Selamat! Anda");
              lcd.setCursor(0, 1); lcd.print("Lulus Semua Lv!");
              currentLevel = 0;
            } else {
              tampilkanInstruksi();
            }
          } else {
            jawabanSalah();
            delay(800);
            tampilkanInstruksi();
          }

          chordBuffer = ""; // Reset buffer chord untuk percobaan berikutnya
        }
      }
    }
  }
}

void tampilkanInstruksi() {
  lcd.clear();
  digitalWrite(ledHijau, LOW);
  digitalWrite(ledMerah, LOW);

  if(currentLevel == 1) {
    lcd.setCursor(0, 0); lcd.print("Tekan Nada:");
    lcd.setCursor(0, 1); lcd.print(namaNada[targetNada]);
  } else if (currentLevel == 2) {
    lcd.setCursor(0, 0); lcd.print(namaChord[targetChord]);
    lcd.setCursor(0, 1); lcd.print(petunjukChord[targetChord]);
  }
}

void jawabanBenar() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Status:");
  lcd.setCursor(0, 1); lcd.print("BENAR!");
  digitalWrite(ledHijau, HIGH);
  digitalWrite(ledMerah, LOW);
}

void jawabanSalah() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Status:");
  lcd.setCursor(0, 1); lcd.print("SALAH!");
  digitalWrite(ledMerah, HIGH);
  digitalWrite(ledHijau, LOW);
}