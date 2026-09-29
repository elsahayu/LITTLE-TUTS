#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int pinTuts[8] = {13, 12, 14, 27, 26, 25, 33, 32};
const char* namaNada[8] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do'"};
const int frekuensi[8] = {262, 294, 330, 349, 392, 440, 494, 523};

const int ledHijau = 4;
const int ledMerah = 2;
const int pinBuzzer = 15;

int targetNada = 0;

void tampilkanInstruksi();
void jawabanBenar();
void jawabanSalah();

void setup() {
  lcd.init();
  lcd.backlight();
  
  pinMode(ledHijau, OUTPUT);
  pinMode(ledMerah, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);

  for(int i = 0; i < 8; i++) {
    pinMode(pinTuts[i], INPUT_PULLUP);
  }

  tampilkanInstruksi();
}

void loop() {
  for(int i = 0; i < 8; i++) {
    if(digitalRead(pinTuts[i]) == LOW) { 
      tone(pinBuzzer, frekuensi[i], 300); 
      
      if(i == targetNada) {
        jawabanBenar();
        targetNada = (targetNada + 1) % 8;
        delay(1500);
        tampilkanInstruksi();
      } else {
        jawabanSalah();
        delay(1500);
        tampilkanInstruksi();
      }
      
      while(digitalRead(pinTuts[i]) == LOW) { delay(10); } 
    }
  }
}

void tampilkanInstruksi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Instruksi: Tekan");
  lcd.setCursor(0, 1);
  lcd.print(namaNada[targetNada]);
  digitalWrite(ledHijau, LOW);
  digitalWrite(ledMerah, LOW);
}

void jawabanBenar() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status:");
  lcd.setCursor(0, 1);
  lcd.print("BENAR!");
  digitalWrite(ledHijau, HIGH);
  digitalWrite(ledMerah, LOW);
}

void jawabanSalah() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status:");
  lcd.setCursor(0, 1);
  lcd.print("SALAH!");
  digitalWrite(ledMerah, HIGH);
  digitalWrite(ledHijau, LOW);
}