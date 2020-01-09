#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Reference binary data for a 16-bit, 16000hz WAV PCM file
//static uint8_t wav_header[44] = {
//    0x52, 0x49, 0x46, 0x46, //RIFF signature
//    0x24, 0x7d, 0x00, 0x00, //ChunkSize, which is filesize - (ChunkID and Chunksize)
//    0x57, 0x41, 0x56, 0x45, //WAVE format
//
//    0x66, 0x6d, 0x74, 0x20, //'fmt '
//    0x10, 0x00, 0x00, 0x00, //PCM subchunk size = 16
//    0x01, 0x00,             //PCM
//    0x01, 0x00,             //number of channels
//    0x80, 0x3e, 0x00, 0x00, //sample rate (=16000)
//    0x80, 0x3e, 0x00, 0x00, //byte rate = SampleRate * NumChannels * BitsPerSample / 8, here 16000
//    0x01, 0x00,             //BlockAlign = NumChannels * BitsPerSample/8
//    0x08, 0x00,             //Bits per sample = 8
//
//    0x64, 0x61, 0x74, 0x61, //'data'
//    0x00, 0x7d, 0x00, 0x00//data size in bytes
//};

#define WAVE_FORMAT_PCM	        0x0001	// PCM
//#define WAVE_FORMAT_IEEE_FLOAT 0x0003  // IEEE float
//#define WAVE_FORMAT_ALAW	     0x0006  // 8-bit ITU-T G.711 A-law
//#define WAVE_FORMAT_MULAW	     0x0007  // 8-bit ITU-T G.711 µ-law
//#define WAVE_FORMAT_EXTENSIBLE 0xFFFE  // Determined by SubFormat

#if 0
// NOTE: This is how I would do support arbitrary chunks, but let's hard-code
// the chunk structure for PCM data only since that's all we care about

typedef struct {
    int32_t chunk_id;         // "fmt ", "data"
    int32_t chunk_size;       // size of chunk data in bytes
    int16_t format_tag;       // "WAVE_FORMAT_PCM" (others not supported)
    int16_t channels;         // channel count
    int32_t sample_rate;      // sample rate in blocks per second
    int32_t avg_byte_rate;    // average bytes of data per second
    int16_t block_align;      // data block size in bytes
    int16_t bits_per_sample;  // sample size in bits
} wav_chunk_fmt;

typedef struct {
    int32_t chunk_id;    // "fmt ", "data"
    int32_t chunk_size;  // size of chunk data in bytes
    union {
        wav_chunk_fmt as_fmt;
        void *as_data;
    } chunk_data;
} wav_chunk;
#endif

#define RIFF 0x46464952
#define WAVE 0x45564157
#define FMT_ 0x20746d66
#define DATA 0x61746164

typedef struct {
    int32_t chunk_id;          // should be "RIFF" (0x46464952)
    int32_t chunk_size;        // size of chunk in bytes (including wave_id)
    int32_t wave_id;           // should be "WAVE" (0x45564157)
#if 0
    wav_chunk *wave_chunks;  // general purpose chunks array; we hard-coded
#else
    // Hard-coded chunk structures specifically for PCM data only (not useful
    // for general purpose support of compressed or extensible WAV files)
    struct {
        int32_t chunk_id;        // "fmt "
        int32_t chunk_size;      // size of chunk data in bytes
        int16_t format_tag;      // "WAVE_FORMAT_PCM" (others not supported)
        int16_t channels;        // channel count
        int32_t sample_rate;     // sample rate in blocks per second
        int32_t avg_byte_rate;   // average bytes of data per second
        int16_t block_align;     // data block size in bytes
        int16_t bits_per_sample; // sample size in bits
    } pcm_fmt;

    struct {
        int32_t chunk_id;    // "data"
        int32_t chunk_size;  // size of chunk data in bytes
        void *data;
    } pcm_data;
#endif
} riff_chunk;

#define ARRAY_COUNT(a) (sizeof(a)/sizeof(a[0]))
#define SAMPLE_RATE 16000

int main(int argc, char *argv[])
{
    int channels = 2;
    int sample_rate = 16000;
    int bytes_per_sample = 4;
    int avg_byte_rate = sample_rate * bytes_per_sample * channels;
    int block_align = bytes_per_sample * channels;
    int sample_count = SAMPLE_RATE * channels;

    //--------------------------------------------------------------------------
    // Generate one second of two sine waves, one in each channel (U.S. dial tone)
    //--------------------------------------------------------------------------
    // Channels can be whatever you want, but the test data I'm generating below
    // is hard-coded to assume two channels.
    assert(channels == 2 && "test sample generator assumes exactly two channels");
    // If you want to support an odd number of samples you need to add a padding
    // byte to the end and account for that in the various chunk_size fields.
    assert(sample_count % 2 == 0 && "we're not handling the pad byte, # of samples must be even");

    const double gain = 0.1;  // 0.0 = silence, 1.0 = max volume
    const double amplitude = INT32_MAX * gain;
    const double tau = 6.28318;  // 2 PI
    const double ring1 = 350.0 / SAMPLE_RATE;
    const double ring2 = 440.0 / SAMPLE_RATE;

    int32_t *samples = (int32_t *)calloc(sample_count, sizeof(samples[0]));
    assert(samples && "memory alloc failed");

    double x1 = 0;
    double x2 = 0;
    for (int i = 0; i < sample_count; i += channels)
    {
        samples[i] = (int32_t)(amplitude * sin(x1 * tau));
        samples[i+1] = (int32_t)(amplitude * sin(x2 * tau));
        x1 += ring1;
        x2 += ring2;
    }
    //--------------------------------------------------------------------------

    // RIFF / WAVE header
    riff_chunk riff = { 0 };

    // Size of sample data in bytes
    int samples_size = sample_count * sizeof(samples[0]);
    // Size of riff/wave header and chunk data, minus sample data
    int riff_minus_samples = sizeof(riff) - sizeof(riff.pcm_data.data);
    // Total size is headers + chunk data - sample data pointer + size of sample data
    int total_size = riff_minus_samples + samples_size;
    // Total size - RIFF header's chunk_id and chunk_size fields
    int riff_chunk_size = total_size - sizeof(riff.chunk_id) - sizeof(riff.chunk_size);

    riff.chunk_id                = RIFF;
    riff.chunk_size              = riff_chunk_size;
    riff.wave_id                 = WAVE;
    // WAVE fmt chunk
    riff.pcm_fmt.chunk_id        = FMT_;
    riff.pcm_fmt.chunk_size      = 16;  // size in bytes of six fields below
    riff.pcm_fmt.format_tag      = WAVE_FORMAT_PCM;
    riff.pcm_fmt.channels        = (uint16_t)channels;
    riff.pcm_fmt.sample_rate     = (uint32_t)sample_rate;
    riff.pcm_fmt.avg_byte_rate   = (uint32_t)avg_byte_rate;
    riff.pcm_fmt.block_align     = (uint16_t)block_align;
    riff.pcm_fmt.bits_per_sample = (uint16_t)(bytes_per_sample * 8);
    // WAVE data chunk
    riff.pcm_data.chunk_id       = DATA;
    riff.pcm_data.chunk_size     = samples_size;
    riff.pcm_data.data           = samples;

    FILE* wav_file = fopen("dialtone.wav", "wb");
    fwrite(&riff, riff_minus_samples, 1, wav_file);
    fwrite(riff.pcm_data.data, samples_size, 1, wav_file);
    fclose(wav_file);

    free(samples);
    return 0;
}