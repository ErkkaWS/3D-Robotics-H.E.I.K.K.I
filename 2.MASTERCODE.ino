#include <Arduino.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <driver/i2s.h>

// ==========================================
// 1. ASETUKSET
// ==========================================
unsigned long janoKestoMs = 60000; 

// ==========================================
// 2. PINNIT
// ==========================================
const int PIN_PAASERVO = 13;
const int PIN_KASISERVO = 12; 
const int PIN_ELEKTRODI = 14;  
const int ledPinnit[10] = {23, 22, 21, 19, 18, 5, 17, 16, 4, 15};

#define I2S_DOUT      27 
#define I2S_BCLK      26
#define I2S_LRC       25
#define SAMPLE_RATE   44100 

// ==========================================
// 3. SERVON ARVOT
// ==========================================
Servo PAASERVO;
Servo KASISERVO;

const int PAA_NUKKUU_US = 500;
const int PAA_ILOINEN_US = 2500;

const int KASI_YLA_US = 2095; 
const int KASI_ALA_US = 1005; 
const int LIIKE_VIIVE_MS = 400; 

unsigned long janoAjastin = 0;

// ==========================================
// 4. ÄÄNI- JA LIIKEFUNKTIOT
// ==========================================

void initI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pin_config = {.bck_io_num = I2S_BCLK, .ws_io_num = I2S_LRC, .data_out_num = I2S_DOUT, .data_in_num = I2S_PIN_NO_CHANGE};
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void soitaWAV(const char* tiedostonimi) {
    if (!LittleFS.exists(tiedostonimi)) return;
    File tiedosto = LittleFS.open(tiedostonimi, "r");
    if (!tiedosto) return;
    
    tiedosto.seek(44); 
    size_t bytes_written;
    uint8_t buff[1024];
    
    while (tiedosto.available()) {
        int bytes_read = tiedosto.read(buff, sizeof(buff));
        int16_t* samples = (int16_t*)buff;
        int num_samples = bytes_read / 2;
        
        for (int i = 0; i < num_samples; i++) {
            int32_t amplified = samples[i] * 3; // 3x Vahvistus
            if (amplified > 32767) amplified = 32767;
            if (amplified < -32768) amplified = -32768;
            samples[i] = (int16_t)amplified;
        }
        i2s_write(I2S_NUM_0, buff, bytes_read, &bytes_written, portMAX_DELAY);
    }
    tiedosto.close();
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void kuorsaaKahdesti() {
    for (int i = 0; i < 2; i++) {
        soitaWAV("/UNI.wav");
        delay(300); 
    }
}

void kopsutaPaahan() {
    for (int i = 0; i < 5; i++) {
        KASISERVO.writeMicroseconds(KASI_YLA_US); 
        delay(250); 
        KASISERVO.writeMicroseconds(KASI_YLA_US - 400); 
        delay(250);
    }
    KASISERVO.writeMicroseconds(KASI_ALA_US); 
}

void slurp2KertaaTask(void *pvParameters) {
    for (int i = 0; i < 2; i++) { soitaWAV("/SLURP.wav"); }
    vTaskDelete(NULL); 
}

void paivitaStatusBar(int taso) {
    for (int i = 0; i < 10; i++) {
        digitalWrite(ledPinnit[i], (i < taso) ? HIGH : LOW);
    }
}

// ==========================================
// 5. SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    pinMode(PIN_ELEKTRODI, INPUT_PULLUP);
    for (int i = 0; i < 10; i++) { pinMode(ledPinnit[i], OUTPUT); }

    if (!LittleFS.begin(true)) return;
    initI2S();

    ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
    PAASERVO.attach(PIN_PAASERVO, 500, 2500);
    KASISERVO.attach(PIN_KASISERVO, 500, 2500);

    paivitaStatusBar(0);
    PAASERVO.writeMicroseconds(PAA_NUKKUU_US);
    KASISERVO.writeMicroseconds(KASI_ALA_US);
    
    delay(4000); 
    PAASERVO.writeMicroseconds(PAA_ILOINEN_US);
    delay(1500); 
    soitaWAV("/BURP.wav");
    PAASERVO.writeMicroseconds(PAA_NUKKUU_US);
    delay(1000);
    kuorsaaKahdesti();

    janoAjastin = millis();
}

// ==========================================
// 6. LOOP
// ==========================================
void loop() {
    // A. JUOMINEN
    if (digitalRead(PIN_ELEKTRODI) == LOW) {
        paivitaStatusBar(0); 
        delay(2000); 
        PAASERVO.writeMicroseconds(PAA_ILOINEN_US);
        delay(1500); 

        xTaskCreate(slurp2KertaaTask, "Slurp", 5000, NULL, 1, NULL);
        delay(500); 
        for (int i = 0; i < 5; i++) {
            KASISERVO.writeMicroseconds(KASI_YLA_US); delay(LIIKE_VIIVE_MS);
            KASISERVO.writeMicroseconds(KASI_ALA_US); delay(LIIKE_VIIVE_MS);
        }

        delay(1000);
        soitaWAV("/BURP.wav"); 
        delay(1000);
        PAASERVO.writeMicroseconds(PAA_NUKKUU_US);
        delay(1500); 
        kuorsaaKahdesti(); 

        while (digitalRead(PIN_ELEKTRODI) == LOW) { delay(100); }
        janoAjastin = millis(); 
    } 
    else {
        // B. JANON KERTYMINEN
        unsigned long kulunutAika = millis() - janoAjastin;
        int janoTaso = map(kulunutAika, 0, janoKestoMs, 0, 10);
        janoTaso = constrain(janoTaso, 0, 10);
        paivitaStatusBar(janoTaso);

        // C. ANELU-SYKLI
        if (janoTaso >= 10) {
            bool saatiinJuomaa = false;

            for (int kertoja = 0; kertoja < 3; kertoja++) {
                PAASERVO.writeMicroseconds(PAA_ILOINEN_US); 
                delay(1200); 
                soitaWAV("/KALJAA.wav"); 
                kopsutaPaahan(); 

                unsigned long taukoAlku = millis();
                while (millis() - taukoAlku < 5000) {
                    if (digitalRead(PIN_ELEKTRODI) == LOW) {
                        saatiinJuomaa = true;
                        break;
                    }
                    delay(10);
                }
                if (saatiinJuomaa) break;
            }

            // D. PETTYMYS (KORJATTU JÄRJESTYS)
            if (!saatiinJuomaa) {
                // 1. ÄÄNI: NOHOH ensin
                soitaWAV("/NOHOH.wav");
                
                // 2. TAUKO: Anna äänen loppua ja virran tasaantua
                delay(500); 

                // 3. LIIKE: Pää alas nukkumisasentoon vasta nyt
                PAASERVO.writeMicroseconds(PAA_NUKKUU_US);
                
                paivitaStatusBar(0);
                janoAjastin = millis(); // Resetti alusta
                
                delay(1500); // Odota että liike on ohi ennen kuorsausta
                kuorsaaKahdesti();
            }
        }
    }
}
