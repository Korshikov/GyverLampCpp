#include "SoundEffect.h"

#include <math.h>
#if defined(ESP32)
#include <driver/adc.h>
#endif
#include <arduinoFFT.h>

namespace  {

#if defined(ESP32)
adc1_channel_t channel = ADC1_CHANNEL_0;
double noiseFilter = 512;
#else
double noiseFilter = 0;
#endif

#define SAMPLES 256
int samplingFrequency = 40000;
#define EQBANDS 8
uint16_t bands = EQBANDS;

unsigned int sampling_period_us;

unsigned long newTime;

struct eqBand {
    uint16_t amplitude;
    byte bandWidth;
    int peak;
    int lastpeak;
    int curval;
    int lastval;
    unsigned long lastmeasured;
};

eqBand audiospectrum[EQBANDS] = {
    /*
                 Adjust the amplitude/bandWidth values
                 to fit your microphone
              */
    { 1000, 2,   0, 0, 0, 0, 0}, // 125
    { 500,  2,   0, 0, 0, 0, 0}, // 250
    { 300,  3,   0, 0, 0, 0, 0}, // 500
    { 250,  7,   0, 0, 0, 0, 0}, // 1k
    { 100,  14,  0, 0, 0, 0, 0}, // 2k
    { 100,  24,  0, 0, 0, 0, 0}, // 4k
    { 100,  48,  0, 0, 0, 0, 0}, // 8k
    { 100,  155, 0, 0, 0, 0, 0}  // 16k
};

/* store bandwidth variations when sample rate changes */
int bandWidth[EQBANDS] = {
    0, 0, 0, 0, 0, 0, 0, 0
};

} // namespace

SoundEffect::SoundEffect()
{
#if defined(ESP32)
    adc1_config_width(ADC_WIDTH_BIT_12);   //Range 0-1023
    adc1_config_channel_atten(channel, ADC_ATTEN_DB_11); //ADC_ATTEN_DB_11 = 0-3,6V
#endif
    sampling_period_us = round(1000000 * (1.0 / samplingFrequency));
    delay(1000);
    setBandwidth();
}

void SoundEffect::tick()
{
    double* vReal = new double[SAMPLES]();
    double* vImag = new double[SAMPLES]();

    for (int i = 0; i < SAMPLES; i++) {
        newTime = micros();
#if defined(ESP32)
        vReal[i] = adc1_get_raw(channel); // A raw conversion takes about 20uS on an ESP32
        delayMicroseconds(20);
#else
        vReal[i] = analogRead(A0); // A conversion takes about 1uS on an ESP32
#endif
        vImag[i] = 0;

        while ((micros() - newTime) < sampling_period_us) {
            // do nothing to wait
            yield();
        }
    }

    arduinoFFT FFT(vReal, vImag, SAMPLES, samplingFrequency);
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();

    delete[] vImag;

    myMatrix->clear();
    int readBands[EQBANDS] = {0};
    for (int i = 2; i < (SAMPLES / 2); i++) { // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency and its value the amplitude.
        if (vReal[i] > noiseFilter) { // Add a crude noise filter, 10 x amplitude or more
            byte bandNum = getBand(i);
            int read = (int)vReal[i];
            if (bandNum != bands && readBands[bandNum] < read) {
                readBands[bandNum] = read;
            }
        }
    }

    delete[] vReal;

//    Serial.print(F("E: "));
    for (int bandNum = 0; bandNum < EQBANDS; bandNum++) {
        if (readBands[bandNum] > 0 && audiospectrum[bandNum].curval != readBands[bandNum]) {
            audiospectrum[bandNum].curval = readBands[bandNum];
        }
        displayBand(bandNum, audiospectrum[bandNum].curval);

//        Serial.printf_P(PSTR("%05d %02d "), audiospectrum[bandNum].curval, audiospectrum[bandNum].lastval);
    }
//    Serial.println();


}

void SoundEffect::displayBand(int band, int dsize)
{
    int dmax = mySettings->matrixSettings.height;
    int ssize = dsize;
    int fsize = dsize / audiospectrum[band].amplitude;
    double factor = settings.scale / 100.0;
    dsize = fsize * factor;
//    Serial.printf("displayBand %d, %d, %d, %f, %d\n", band, ssize, fsize, factor, dsize);
    if (dsize > dmax) {
        dsize = dmax;
    }
    for (int y = 0; y < dsize; y++) {
        myMatrix->drawPixelXY(band * 2, y, CRGB(CRGB::Blue));
        myMatrix->drawPixelXY(band * 2 + 1, y, CRGB(CRGB::Blue));
    }
    audiospectrum[band].lastval = dsize;
    audiospectrum[band].lastmeasured = millis();

//    Serial.printf_P(PSTR("E%d %05d %02d\n"), band, audiospectrum[band].curval, dsize);
}

void SoundEffect::setBandwidth()
{
    byte multiplier = SAMPLES / 256;
    bandWidth[0] = audiospectrum[0].bandWidth * multiplier;
    for (byte j = 1; j < EQBANDS; j++) {
        bandWidth[j] = audiospectrum[j].bandWidth * multiplier + bandWidth[j - 1];
    }
}


byte SoundEffect::getBand(int i)
{
    for (byte j = 0; j < EQBANDS; j++) {
        if (i <= bandWidth[j]) {
            return j;
        }
    }
    return EQBANDS;
}

