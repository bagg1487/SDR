#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

int main()
{
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");        
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");          
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); 
    }
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL) {
        printf("Failed to create SDR device\n");
        return -1;
    }

    double sample_rate = 1e6;
    double carrier_freq = 800e6;

    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);

    SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, sample_rate);
    SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    size_t channels[] = {0};
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);

    SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, 10.0);
    SoapySDRDevice_setGain(sdr, SOAPY_SDR_TX, 0, -90.0);

    SoapySDRKwargs stream_args = {};
    SoapySDRKwargs_set(&stream_args, "timestamp_every", "1920");

    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, &stream_args);
    SoapySDRStream *txStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_TX, SOAPY_SDR_CS16, channels, channel_count, &stream_args);
    
    SoapySDRKwargs_clear(&stream_args);

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    SoapySDRDevice_activateStream(sdr, txStream, 0, 0, 0);

    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    size_t tx_mtu = SoapySDRDevice_getStreamMTU(sdr, txStream);

    int16_t *tx_buff = (int16_t *)malloc(2 * tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t *)malloc(2 * rx_mtu * sizeof(int16_t));

    const long timeoutUs = 1000000;
    long long last_time = 0;
    long long total_samples_rx = 0;
    size_t iteration_count = 10;

    for (size_t i = 0; i < 2 * tx_mtu; i += 2)
    {
        tx_buff[i] = 1500 << 4;
        tx_buff[i + 1] = 1500 << 4;
    }

    for (size_t buffers_read = 0; buffers_read < iteration_count; buffers_read++)
    {
        void *rx_buffs[] = {rx_buffer};
        int flags = 0;
        long long timeNs = 0;

        int sr = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, rx_mtu, &flags, &timeNs, timeoutUs);

        if (sr > 0) {
            if (timeNs == 0) {
                timeNs = (long long)((double)total_samples_rx / sample_rate * 1e9);
            }
            total_samples_rx += sr;
        }

        printf("Buffer: %lu - Samples: %i, Flags: %i, Time: %lli, TimeDiff: %lli\n", 
               (unsigned long)buffers_read, sr, flags, timeNs, (last_time == 0) ? 0 : (timeNs - last_time));
        
        last_time = timeNs;

        if (buffers_read == 2)
        {
            void *tx_buffs[] = {tx_buff};
            int tx_flags = 0;
            
            int st = SoapySDRDevice_writeStream(sdr, txStream, (const void * const*)tx_buffs, tx_mtu, &tx_flags, 0, timeoutUs);
            
            printf("TX Sent - Status: %i\n", st);
        }
    }

    free(tx_buff);
    free(rx_buffer);

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0);

    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_closeStream(sdr, txStream);

    SoapySDRDevice_unmake(sdr);

    return 0;
}