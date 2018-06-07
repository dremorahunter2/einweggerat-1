#ifndef AUDIO_H
#define AUDIO_H

#include <list>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "audio/audio_resampler.h"
#include "../3rdparty/mini_al.h"
#include "../3rdparty/libretro.h"
#include "queues/fifo_queue.h"
#include "rthreads/rthreads.h"


#ifdef __cplusplus
extern "C" {
#endif

  void *resampler_sinc_init(double bandwidth_mod);
  void resampler_sinc_process(void *re_, struct resampler_data *data);
  void resampler_sinc_free(void *re_);

  long long microseconds_now();
  long long milliseconds_now();

  class Audio
  {

  public:
    bool init(double refreshra, retro_system_av_info av);
    void destroy();
    void reset();
    void sleeplil();
    int get_clientrate();
    void mix(const int16_t* samples, size_t sample_count);
    mal_uint32 fill_buffer(uint8_t* pSamples, mal_uint32 samplecount);
    mal_context context;
    mal_device device;
    unsigned client_rate;
    fifo_buffer* _fifo;
    float fps;
    double system_fps;
    double skew;
    double system_rate;
    double resamp_original;
    void* resample;
    float *input_float;
    float *output_float;
    slock_t *lockz;
    scond_t *condz;
  };


#ifdef __cplusplus
}
#endif

#endif AUDIO_H