#define MAL_IMPLEMENTATION
#define MAL_NO_WASAPI
#define MAL_NO_WINMM
#include "audio.h"
#include <initguid.h>
#include <Mmdeviceapi.h>
using namespace std;

#define FRAME_COUNT (1024)

static mal_uint32 audio_callback(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
  //convert from samples to the actual number of bytes.
  int count_bytes = frameCount * pDevice->channels * mal_get_sample_size_in_bytes(mal_format_f32);
  int ret = ((Audio*)pDevice->pUserData)->fill_buffer((uint8_t*)pSamples, count_bytes) / mal_get_sample_size_in_bytes(mal_format_f32);
  return ret / pDevice->channels;
  //...and back to samples
}

static retro_time_t frame_limit_minimum_time = 0.0;
static retro_time_t frame_limit_last_time = 0.0;

long long milliseconds_now() {
  static LARGE_INTEGER s_frequency;
  static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
  if (s_use_qpc) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (1000LL * now.QuadPart) / s_frequency.QuadPart;
  }
  else {
    return GetTickCount();
  }
}

long long microseconds_now() {
  static LARGE_INTEGER s_frequency;
  static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
  if (s_use_qpc) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (1000000LL * now.QuadPart) / s_frequency.QuadPart;
  }
  else {
    return GetTickCount();
  }
}

int Audio::get_clientrate()
{
   HRESULT hr;
   IMMDevice * pDevice = NULL;
   IMMDeviceEnumerator * pEnumerator = NULL;
   IPropertyStore* store = nullptr;
   PWAVEFORMATEX deviceFormatProperties;
   PROPVARIANT prop;
   CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID *)&pEnumerator);
   // get default audio endpoint
   pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
   pDevice->OpenPropertyStore(STGM_READ, &store);
   store->GetValue(PKEY_AudioEngine_DeviceFormat, &prop);
   deviceFormatProperties = (PWAVEFORMATEX)prop.blob.pBlobData;
   return deviceFormatProperties->nSamplesPerSec;
}

bool Audio::init(double refreshra, retro_system_av_info av)
{
  condz = scond_new();
  lockz = slock_new();
  system_rate = av.timing.sample_rate;
  system_fps = av.timing.fps;


  if (refreshra > 119.0 && refreshra < 121.0)
  refreshra /= 2.0;
  if (refreshra > 179.0 && refreshra < 181.0)
  refreshra /= 3.0;
  if (refreshra > 239.0 && refreshra < 241.0)
  refreshra /= 4.0;


  if (fabs(1.0f - system_fps / refreshra) <= 0.05)
     system_rate *= (refreshra / system_fps);
  client_rate = get_clientrate();
  resamp_original = (client_rate / system_rate);
  resample = resampler_sinc_init(resamp_original);
  if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
    printf("Failed to initialize context.");
    return false;
  }
  mal_device_config config = mal_device_config_init_playback(mal_format_f32, 2, client_rate, audio_callback);
  config.bufferSizeInFrames = (FRAME_COUNT);
  if (mal_device_init(&context, mal_device_type_playback, NULL, &config, this, &device) != MAL_SUCCESS) {
    mal_context_uninit(&context);
    return false;
  }
  size_t sampsize = mal_device_get_buffer_size_in_bytes(&device);
  _fifo = fifo_new(sampsize * 4); //number of bytes
  output_float = new float[sampsize * 2]; //spare space for resampler
  input_float = new float[FRAME_COUNT * 4];
  mal_device_start(&device);
  frame_limit_last_time = microseconds_now();
  frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f / (av.timing.fps));
  return true;
}
void Audio::destroy()
{
  {
    mal_device_stop(&device);
    mal_context_uninit(&context);
    fifo_free(_fifo);
    slock_free(lockz);
    scond_free(condz);
    delete[] input_float;
    delete[] output_float;
    resampler_sinc_free(resample);
  }
}
void Audio::reset()
{
}


void Audio::sleeplil()
{
  retro_time_t to_sleep_ms = ((frame_limit_last_time + frame_limit_minimum_time) - microseconds_now()) / 1000;
  if (to_sleep_ms > 0)
  {
    float sleep_ms = (unsigned)to_sleep_ms;
    /* Combat jitter a bit. */
    frame_limit_last_time += frame_limit_minimum_time;
    Sleep(sleep_ms);
    return;
  }
  frame_limit_last_time = microseconds_now();
}

void Audio::mix(const int16_t* samples, size_t size)
{
  uint32_t in_len = size * 2;
  int    half_size = (int)_fifo->size / 2;
  double drc_ratio = resamp_original * (1.0 + 0.005 *
    (((int)fifo_write_avail(_fifo) - half_size) / (double)half_size));
  mal_pcm_s16_to_f32(input_float, samples, in_len);

  struct resampler_data src_data = { 0 };
  src_data.input_frames = size;
  src_data.ratio = drc_ratio;
  src_data.data_in = input_float;
  src_data.data_out = output_float;
  resampler_sinc_process(resample, &src_data);
  int out_len = src_data.output_frames * 2 * sizeof(float);

  size_t written = 0;
  while (written < out_len)
  {
    slock_lock(lockz);
    size_t avail = fifo_write_avail(_fifo);
    if (avail)
    {
      size_t write_amt = out_len - written > avail ? avail : out_len - written;
      fifo_write(_fifo, (const char*)output_float + written, write_amt);
      written += write_amt;
      slock_unlock(lockz);
    }
    else
    {
      scond_wait(condz, lockz);
      slock_unlock(lockz);
      continue;

    }
  }
}

mal_uint32 Audio::fill_buffer(uint8_t* out, mal_uint32 count) {
  slock_lock(lockz);
  size_t amount = fifo_read_avail(_fifo);
  amount = count > amount ? amount : count;
  fifo_read(_fifo, out, amount);
  scond_signal(condz);
  slock_unlock(lockz);
  memset(out + amount, 0, count - amount);
  return count;
}

