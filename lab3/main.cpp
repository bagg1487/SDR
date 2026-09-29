#include <SoapySDR/Device.h>   // Инициализация устройства
#include <SoapySDR/Formats.h>  // Типы данных, используемых для записи сэмплов
#include <stdio.h>             // printf, fopen, fwrite, fclose
#include <stdlib.h>            // free, malloc
#include <stdint.h>
#include <complex.h>

int main(int argc, char *argv[]) {

SoapySDRKwargs args = {};
SoapySDRKwargs_set(&args, "driver", "plutosdr");        // Говорим какой Тип устройства 
if (1) {
    SoapySDRKwargs_set(&args, "uri", "usb:");           // Способ обмена сэмплами (по USB)
} else {
    SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); // Или (по IP-адресу)
}
SoapySDRKwargs_set(&args, "direct", "1");               // 
SoapySDRKwargs_set(&args, "timestamp_every", "1920");   // Размер буфера + временные метки
SoapySDRKwargs_set(&args, "loopback", "0");             // Используем антенны или нет
SoapySDRDevice *sdr = SoapySDRDevice_make(&args);       // Инициализация
SoapySDRKwargs_clear(&args);

int sample_rate = 1e6;
int carrier_freq = 800e6;
// Параметры RX части
SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq , NULL);

// Параметры TX части
SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq , NULL);

// Инициализация количества каналов RX\TX (в AdalmPluto он один, нулевой)
size_t channels[] = {0};
// Настройки усилителей на RX\TX (Исправлено: передаем индекс канала 0 вместо массива)
SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, 10.0); // Чувствительность приемника
SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, 0, -90.0);// Усиление передатчика

size_t channel_count = sizeof(channels) / sizeof(channels[0]);
// Формирование потоков для передачи и приема сэмплов
SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, NULL);

SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0); //start streaming

// Получение MTU (Maximum Transmission Unit), в нашем случае - размер буферов. 
size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

// Выделяем память под буферы RX и TX
int16_t tx_buff[2*tx_mtu];
int16_t rx_buffer[2*rx_mtu];

// Заполнение tx_buff значениями сэмплов: первые 16 бит - I, вторые - Q
for (size_t i = 0; i < tx_mtu; i++) {
    tx_buff[2 * i]     = 1000; // I
    tx_buff[2 * i + 1] = 0;    // Q
}

const long  timeoutUs = 400000;
long long last_time = 0;
// Количество итераций чтения из буфера
size_t iteration_count = 10;

// Открываем файл txdata.pcm для записи в бинарном режиме
FILE *output_file = fopen("txdata.pcm", "wb");

// Начинается работа с получением и отправкой сэмплов
for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
{
    void *rx_buffs[] = {rx_buffer};
    int flags;        // flags set by receive operation
    long long timeNs; // timestamp for receive buffer
    
    // считали буффер RX, записали его в rx_buffer
    int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);

    // Смотрим на количество считаных сэмплов, времени прихода и разницы во времени с чтением прошлого буфера
    printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", buffers_read, sr, flags, timeNs, timeNs - last_time);  
    
    // Если сэмплы считались и файл открыт — пишем их на диск
    if (sr > 0 && output_file != NULL) {
        fwrite(rx_buffer, sizeof(int16_t) * 2, sr, output_file);
    }

    // Отправка данных в эфир (TX)
    const void *tx_buffs[] = {tx_buff};
    int tx_flags = 0;
    int st = SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, tx_mtu, &tx_flags, timeNs, timeoutUs);
    if (st < 0) {
        printf("Ошибка записи в поток TX: %i\n", st);
    }
}

// Закрываем файл после выхода из цикла
if (output_file != NULL) {
    fclose(output_file);
    printf("Файл txdata.pcm успешно сохранен!\n");
}

return 0;
}
