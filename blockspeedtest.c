/**
 * block-speed-test.c
 *
 * Copyright (c) 2017-2019 TiePie engineering
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#ifdef __WINNT__
  #include <windows.h>
#else
  #include <poll.h>
  #include <sys/eventfd.h>
  #include <time.h>
#endif
#include <string.h>
#include <libtiepie.h>

#define EVENT_COUNT 2

#define DEVICE_GONE \
  { \
    fprintf(stderr, "Device gone!\n"); \
    status = EXIT_FAILURE; \
    goto exit; \
  }

#define DATA_READY \
  { \
    if(raw) \
      ScpGetDataRaw(scp, buffers, channel_count, 0, record_length); \
    else \
      ScpGetData(scp, (float**)buffers, channel_count, 0, record_length); \
    \
    if(LibGetLastStatus() < LIBTIEPIESTATUS_SUCCESS) \
    { \
      fprintf(stderr, "%s failed: %s\n", raw ? "ScpGetDataRaw" : "ScpGetData", LibGetLastStatusStr()); \
      status = EXIT_FAILURE; \
      goto exit; \
    } \
    n--; \
  }

int main(int argc, char* argv[])
{
  int status = EXIT_SUCCESS;
  unsigned int resolution = 0;
  unsigned int active_channel_count = 0;
  double sample_frequency = 1e6; // 1 MHz
  uint64_t record_length = 5000; // 5 kS
  unsigned int num_measurements = 100;
  bool raw = false;
  bool ext_trig = false;
  uint32_t serial_number = 0;

  {
    int opt;
    while((opt = getopt(argc, argv, "b:c:f:l:n:s:reh")) != -1)
    {
      switch(opt)
      {
        case 'b':
          sscanf(optarg, "%u", &resolution);
          break;

        case 'c':
          sscanf(optarg, "%u", &active_channel_count);
          break;

        case 'f':
          sscanf(optarg, "%lf", &sample_frequency);
          break;

        case 'l':
          sscanf(optarg, "%" PRIu64, &record_length);
          break;

        case 'n':
          sscanf(optarg, "%u", &num_measurements);
          break;

        case 's':
          sscanf(optarg, "%" PRIu32, &serial_number);
          break;

        case 'r':
          raw = true;
          break;

        case 'e':
          ext_trig = true;
          break;

        case 'h':
        default:
          fprintf(stderr, "Usage: %s [-b resolution] [-c active channel count] [-f sample frequency] [-l record length] [-n number of measurements] [-s serial number] [-r] [-e]\n", argv[0]);
          exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
      }
    }
  }

  //===========================================================================

  TpVersion_t version = LibGetVersion();
  printf("libtiepie v%u.%u.%u.%u%s\n" ,
          (uint16_t)TPVERSION_MAJOR(version) ,
          (uint16_t)TPVERSION_MINOR(version) ,
          (uint16_t)TPVERSION_RELEASE(version) ,
          (uint16_t)TPVERSION_BUILD(version) ,
          LibGetVersionExtra());

  LibInit();

  // Open device:
  LstUpdate();

  LibTiePieHandle_t scp;
  if(serial_number != 0)
    scp = LstOpenOscilloscope(IDKIND_SERIALNUMBER, serial_number);
  else
    scp = LstOpenOscilloscope(IDKIND_INDEX, 0);

  if(scp == LIBTIEPIE_HANDLE_INVALID)
  {
    fprintf(stderr, "LstOpenOscilloscope failed: %s\n", LibGetLastStatusStr());
    status = EXIT_FAILURE;
    goto exit_no_mem;
  }

  const uint16_t channel_count = ScpGetChannelCount(scp);

  // Setup device:
  ScpSetMeasureMode(scp, MM_BLOCK);

  if(active_channel_count == 0 || active_channel_count > channel_count)
    active_channel_count = channel_count;
  printf("Active channel count: %u\n", active_channel_count);
  for(uint16_t i = 0; i < channel_count; i++)
  {
    printf("  Ch%u: %s\n", i + 1, ScpChSetEnabled(scp, i, i < active_channel_count ? BOOL8_TRUE : BOOL8_FALSE) ? "enabled" : "disabled");
    printf("  Ch%u trigger: %s\n", i + 1, ScpChTrSetEnabled(scp, i, BOOL8_FALSE)  ? "enabled" : "disabled");
  }

  if(resolution != 0)
    ScpSetResolution(scp, resolution);

  // Setup trigger:
  if(ext_trig)
  {
    const uint16_t ext1 = DevTrGetInputIndexById(scp, TIID_EXT1);
    if(ext1 == LIBTIEPIE_TRIGGERIO_INDEX_INVALID ||
        DevTrInSetEnabled(scp, ext1, BOOL8_TRUE) != BOOL8_TRUE ||
        DevTrInSetKind(scp, ext1, TK_FALLINGEDGE) != TK_FALLINGEDGE)
    {
      fprintf(stderr, "Can't setup trigger input EXT1\n");
      status = EXIT_FAILURE;
      goto exit_no_mem;
    }
    printf("Enabled EXT1 falling edge trigger\n");
    ScpSetTriggerTimeOut(scp, TO_INFINITY);
    printf("Disabled trigger timeout\n");
  }
  else
  {
    printf("Disabled trigger system\n");
    ScpSetTriggerTimeOut(scp, 0);
  }

  sample_frequency = ScpSetSampleFrequency(scp, sample_frequency);
  printf("Sample frequency: %f MHz\n", sample_frequency / 1e6);

  printf("Resolution: %u bit\n", ScpGetResolution(scp));

  record_length = ScpSetRecordLength(scp, record_length);
  printf("Record length: %" PRIu64 " Samples\n", record_length);

  printf("Measurement duration: %f s\n", record_length / sample_frequency);

  printf("Data type: %s\n", raw ? "raw" : "float");

  {
    // Alloc memory:
    const size_t sample_size = raw ? ceil(ScpGetResolution(scp) / 8.0) : sizeof(float);
    void* buffers[channel_count];
    for(uint16_t i = 0; i < channel_count; i++)
      if(i < active_channel_count)
        buffers[i] = malloc(record_length * sample_size);
      else
        buffers[i] = NULL;

    // Setup events:
#ifdef __WINNT__
    HANDLE events[EVENT_COUNT] = {
      CreateEventA(NULL, FALSE, FALSE, NULL),
      CreateEventA(NULL, FALSE, FALSE, NULL)
    };

    DevSetEventRemoved(scp, events[0]);
    ScpSetEventDataReady(scp, events[1]);
#else
    int fd_removed = eventfd(0, EFD_NONBLOCK);
    int fd_data_ready = eventfd(0, EFD_NONBLOCK);

    DevSetEventRemoved(scp, fd_removed);
    ScpSetEventDataReady(scp, fd_data_ready);

    struct pollfd fds[EVENT_COUNT] = {
      {fd: fd_removed, events: POLLIN, revents: 0},
      {fd: fd_data_ready, events: POLLIN, revents: 0}
    };
#endif

    unsigned int n = num_measurements;

    printf("\nPerforming measurements...\n");

#ifdef __WINNT__
    DWORD r;
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    QueryPerformanceCounter(&start);
#else
    int r;
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

    while(n != 0)
    {
      if(!ScpStart(scp))
      {
        fprintf(stderr, "ScpStart failed: %s\n", LibGetLastStatusStr());
        status = EXIT_FAILURE;
        goto exit;
      }

#ifdef __WINNT__
      r = WaitForMultipleObjects(EVENT_COUNT, events, FALSE, INFINITE);

      if(r == WAIT_OBJECT_0)
      {
        DEVICE_GONE
      }
      else if(r == WAIT_OBJECT_0 + 1)
      {
        DATA_READY
      }
      else
      {
        fprintf(stderr, "WaitForMultipleObjects() returned: %lu\n", r);
        status = EXIT_FAILURE;
        goto exit;
      }
#else
      if((r = poll(fds, EVENT_COUNT, -1)) <= 0)
      {
        fprintf(stderr, "poll() returned: %d\n", r);
        status = EXIT_FAILURE;
        goto exit;
      }

      for(int i = 0; i < EVENT_COUNT; i++)
      {
        if(fds[i].revents & POLLIN)
        {
          eventfd_t tmp;
          eventfd_read(fds[i].fd, &tmp);

          if(fds[i].fd == fd_removed)
          {
            DEVICE_GONE
          }

          if(fds[i].fd == fd_data_ready)
          {
            DATA_READY
          }
        }

        fds[i].revents = 0;
      }
#endif
    }
#ifdef __WINNT__
    QueryPerformanceCounter(&end);
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    const double duration = (end.QuadPart - start.QuadPart) / (double)frequency.QuadPart;
#else
    clock_gettime(CLOCK_MONOTONIC, &end);
    const double duration = (end.tv_sec - start.tv_sec) + ((end.tv_nsec - start.tv_nsec) / 1e9);
#endif

    printf("\nResult:\n  Preformed %u measurements in %fs\n  Average of %fs per measurement\n  Average of %f measurements per second\n", num_measurements, duration, duration / num_measurements, num_measurements / duration);

exit:
    for(uint16_t i = 0; i < channel_count; i++)
      free(buffers[i]);
  }

exit_no_mem:
  LibExit();
  return status;
}
