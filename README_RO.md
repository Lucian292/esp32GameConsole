# Consolă de jocuri ESP32

Un framework de consolă de jocuri pentru ESP32, construit cu PlatformIO și testat în Wokwi. Proiectul folosește un display OLED SSD1306 de 128x64, opt butoane, un buzzer și un servo pentru a crea un sistem simplu de meniu, în stil handheld, cu două jocuri jucabile: Snake și Tetris.

Codul este organizat ca un nucleu compact al consolei în `src/main.cpp`, plus module separate pentru fiecare joc. Meniul principal include și un flux persistent de blocare/deblocare a servo-ului, împreună cu ecrane utilitare pentru testarea intrărilor și setări.

## Funcționalități

- Meniu principal cu intrări pentru Snake, Tetris, Settings și Lock
- Ecrane de joc modulare pentru Snake și Tetris
- Interfață OLED SSD1306 128x64 prin I2C
- Intrări de tip active-low, cu debounce
- Feedback audio prin buzzer
- Control servo cu stare persistentă de lock/unlock
- Ecran de test pentru verificarea tuturor butoanelor
- Ecran de setări cu test pentru servo și test pentru buzzer
- Diagramă Wokwi aranjată manual pentru un layout clar și lizibil

## Hardware

Versiunea curentă este destinată unui ESP32 DevKit V1.

| Componentă | Pin / Conexiune | Observații |
| --- | --- | --- |
| OLED SSD1306 | SDA = GPIO 21 | Linie de date I2C |
| OLED SSD1306 | SCL = GPIO 22 | Linie de ceas I2C |
| OLED SSD1306 | VIN = 3V3 | Alimentare la 3.3V |
| OLED SSD1306 | GND = GND | Masă comună |
| OLED SSD1306 | Adresă = 0x3C | Configurată în cod și în Wokwi |
| Buzzer | GPIO 15 | Feedback sonor |
| Servo PWM | GPIO 13 | Actuator pentru blocare/deblocare |
| Servo V+ | VIN | Alimentat de la placa ESP32 |
| Servo GND | GND | Masă comună |
| Buton Up | GPIO 32 | Active-low, cu pull-up intern |
| Buton Down | GPIO 33 | Active-low, cu pull-up intern |
| Buton Left | GPIO 25 | Active-low, cu pull-up intern |
| Buton Right | GPIO 26 | Active-low, cu pull-up intern |
| Buton A | GPIO 27 | Active-low, cu pull-up intern |
| Buton B | GPIO 14 | Active-low, cu pull-up intern |
| Buton Start | GPIO 18 | Active-low, cu pull-up intern |
| Buton Select | GPIO 19 | Active-low, cu pull-up intern |

## Controale

### Meniul principal

- Sus / Jos: navigare între elementele meniului
- A sau Start: deschide elementul selectat
- Lock: forțează servo-ul în poziția blocată

### Snake

- Sus / Jos / Stânga / Dreapta: mișcă șarpele
- B sau Select: revine la meniul principal
- A sau Start în game over: repornește jocul
- B sau Select în game over: revine la meniu

Snake deblochează servo-ul atunci când scorul ajunge la 3.

### Tetris

- Stânga / Dreapta: mișcă piesa curentă
- Sus sau A: rotește piesa
- Jos: soft drop
- Start: hard drop
- B sau Select: revine la meniul principal
- A sau Start în game over: repornește jocul
- B sau Select în game over: revine la meniu

Tetris acordă 100 de puncte pentru fiecare linie eliminată și deblochează servo-ul când scorul ajunge la 100.

### Settings

- Sus / Jos: navigare între opțiuni
- A sau Start: activează opțiunea selectată
- B sau Select: revine la meniul principal

Setările includ momentan:

- Servo test: pulsează servo-ul pentru scurt timp și apoi revine la starea anterioară de blocare/deblocare
- Buzzer test: redă un sunet scurt
- Back: revine la meniul principal

## Observații despre jocuri

### Snake

Snake rulează pe o grilă compactă, adaptată pentru ecranul OLED. Mâncarea crește scorul și declanșează un sunet de feedback. Când este atins pragul de scor, servo-ul este deblocat și rămâne deblocat până când utilizatorul selectează Lock din meniul principal.

Pragul curent de deblocare pentru Snake: 3 puncte.

### Tetris

Tetris folosește o tablă mică, potrivită pentru afișajul OLED. Fiecare linie eliminată crește scorul, declanșează un sunet de feedback și contribuie la pragul de deblocare. Jocul se termină atunci când o piesă nouă nu mai poate apărea, deoarece stiva a ajuns sus.

Pragul curent de deblocare pentru Tetris: 100 de puncte.

## Structura proiectului

```text
.
├── diagram.json
├── platformio.ini
├── wokwi.toml
├── README.md
├── README_RO.md
├── src
│   ├── main.cpp
│   ├── SnakeGame.h
│   ├── SnakeGame.cpp
│   ├── TetrisGame.h
│   └── TetrisGame.cpp
├── include
├── lib
└── test
```

## Build și rulare

### PlatformIO

Deschide proiectul în VS Code cu extensia PlatformIO instalată, apoi compilează firmware-ul.

```bash
pio run
```

Dacă vrei să încarci firmware-ul pe hardware real, folosește fluxul obișnuit de upload din PlatformIO pentru mediul selectat.

### Wokwi

Proiectul este pregătit pentru Wokwi prin `diagram.json` și `wokwi.toml`. Diagrama a fost aranjată manual pentru ca OLED-ul, buzzerul, servo-ul și butoanele să fie ușor de urmărit în simulator.

## Dependențe

Proiectul folosește bibliotecile principale:

- Adafruit SSD1306
- Adafruit GFX Library
- ESP32Servo

Acestea sunt declarate în `platformio.ini`.

## Prezentare generală a implementării

- `src/main.cpp` gestionează display-ul, meniul, scanarea butoanelor, buzzerul și starea servo-ului.
- `src/SnakeGame.*` conține logica completă pentru Snake.
- `src/TetrisGame.*` conține logica completă pentru Tetris.
- `diagram.json` definește toată conexiunea hardware din Wokwi.

Consola este gândită modular, astfel încât pot fi adăugate ușor jocuri sau ecrane noi fără rescrierea stratului hardware.

## Note

- Butoanele sunt conectate active-low și folosesc pull-up intern.
- Starea de blocare a servo-ului este persistentă până când utilizatorul o schimbă explicit.
- Proiectul este gândit să funcționeze bine în Wokwi, dar aceeași hartă de pini se potrivește și cu placa ESP32 folosită de PlatformIO.
