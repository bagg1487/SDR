#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    const int sample_rate = 1000000;
    const int carrier_freq = 800000000;
    const long timeoutUs = 400000;

    SoapySDRKwargs txArgs = {};
    SoapySDRKwargs_set(&txArgs, "driver", "plutosdr");
    SoapySDRKwargs_set(&txArgs, "uri", "ip:192.168.3.1");
    SoapySDRKwargs_set(&txArgs, "direct", "1");
    SoapySDRKwargs_set(&txArgs, "timestamp_every", "1920");
    SoapySDRKwargs_set(&txArgs, "loopback", "0");

    SoapySDRDevice *txSdr =
        SoapySDRDevice_make(&txArgs);

    SoapySDRKwargs_clear(&txArgs);

    if (txSdr == NULL) {
        printf("TX SDR init error\n");
        return 1;
    }

    SoapySDRKwargs rxArgs = {};
    SoapySDRKwargs_set(&rxArgs, "driver", "plutosdr");
    SoapySDRKwargs_set(&rxArgs, "uri", "ip:192.168.2.1");
    SoapySDRKwargs_set(&rxArgs, "direct", "1");
    SoapySDRKwargs_set(&rxArgs, "timestamp_every", "1920");
    SoapySDRKwargs_set(&rxArgs, "loopback", "0");

    SoapySDRDevice *rxSdr =
        SoapySDRDevice_make(&rxArgs);

    SoapySDRKwargs_clear(&rxArgs);

    if (rxSdr == NULL) {
        printf("RX SDR init error\n");
        SoapySDRDevice_unmake(txSdr);
        return 1;
    }

    SoapySDRDevice_setSampleRate(
        txSdr, SOAPY_SDR_TX, 0, sample_rate);

    SoapySDRDevice_setFrequency(
        txSdr, SOAPY_SDR_TX, 0, carrier_freq, NULL);

    SoapySDRDevice_setGain(
        txSdr, SOAPY_SDR_TX, 0, -40.0);

    SoapySDRDevice_setSampleRate(
        rxSdr, SOAPY_SDR_RX, 0, sample_rate);

    SoapySDRDevice_setFrequency(
        rxSdr, SOAPY_SDR_RX, 0, carrier_freq, NULL);

    SoapySDRDevice_setGain(
        rxSdr, SOAPY_SDR_RX, 0, 10.0);

    size_t channels[] = {0};

    SoapySDRStream *txStream =
        SoapySDRDevice_setupStream(
            txSdr,
            SOAPY_SDR_TX,
            SOAPY_SDR_CS16,
            channels,
            1,
            NULL
        );

    SoapySDRStream *rxStream =
        SoapySDRDevice_setupStream(
            rxSdr,
            SOAPY_SDR_RX,
            SOAPY_SDR_CS16,
            channels,
            1,
            NULL
        );

    if (txStream == NULL || rxStream == NULL) {
        printf("Stream init error\n");
        return 1;
    }

    SoapySDRDevice_activateStream(
        rxSdr, rxStream, 0, 0, 0);

    SoapySDRDevice_activateStream(
        txSdr, txStream, 0, 0, 0);

    size_t tx_mtu =
        SoapySDRDevice_getStreamMTU(
            txSdr, txStream);

    size_t rx_mtu =
        SoapySDRDevice_getStreamMTU(
            rxSdr, rxStream);

    printf("TX MTU: %zu\n", tx_mtu);
    printf("RX MTU: %zu\n", rx_mtu);

    int16_t tx_buffer[2 * tx_mtu];
    int16_t rx_buffer[2 * rx_mtu];

    FILE *file = fopen("rxdata.pcm", "wb");

    if (file == NULL) {
        printf("File error\n");
        return 1;
    }

    for (size_t n = 0; n < tx_mtu; n++) {

        size_t i = 2 * n;

        if (n % 2 == 0) {
            tx_buffer[i] = 1500 << 4;
            tx_buffer[i + 1] = 1500 << 4;
        }
        else {
            tx_buffer[i] = 0;
            tx_buffer[i + 1] = 0;
        }
    }

    printf("Start TX -> RX\n");

    for (size_t iteration = 0;
         iteration < 100;
         iteration++) {

        void *tx_buffs[] = {tx_buffer};

        int tx_flags = 0;
        long long tx_time = 0;

        int st = SoapySDRDevice_writeStream(
            txSdr,
            txStream,
            (const void * const *)tx_buffs,
            tx_mtu,
            &tx_flags,
            tx_time,
            timeoutUs
        );

        if (st < 0) {
            printf("TX error: %d\n", st);
        }

        void *rx_buffs[] = {rx_buffer};

        int rx_flags = 0;
        long long rx_time = 0;

        int sr = SoapySDRDevice_readStream(
            rxSdr,
            rxStream,
            rx_buffs,
            rx_mtu,
            &rx_flags,
            &rx_time,
            timeoutUs
        );

        if (sr < 0) {
            printf(
                "RX error: %d\n",
                sr
            );

            continue;
        }

        printf(
            "Buffer: %zu | TX: %d | RX: %d | Time: %lld\n",
            iteration,
            st,
            sr,
            rx_time
        );

        fwrite(
            rx_buffer,
            2 * sizeof(int16_t),
            sr,
            file
        );
    }

    fclose(file);

    SoapySDRDevice_deactivateStream(
        txSdr, txStream, 0, 0);

    SoapySDRDevice_deactivateStream(
        rxSdr, rxStream, 0, 0);

    SoapySDRDevice_closeStream(
        txSdr, txStream);

    SoapySDRDevice_closeStream(
        rxSdr, rxStream);

    SoapySDRDevice_unmake(txSdr);
    SoapySDRDevice_unmake(rxSdr);

    printf("Done\n");

    return 0;
}