#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#include "Version.h"
#include "RewindClient.h"

#define TDMA_FRAME_DURATION   60

#define DSD_MAGIC_TEXT        ".amb"
#define DSD_MAGIC_SIZE        4
#define DSD_AMBE_CHUNK_SIZE   8

#define LINEAR_FRAME_SIZE     7

#define HELPER(value)         #value
#define STRING(value)         HELPER(value)

#define COUNT(array)          sizeof(array) / sizeof(array[0])

#define BUFFER_SIZE           64
#define CLIENT_NAME           "DigestPlay " STRING(VERSION) " " BUILD

int main(int argc, char* argv[])
{
  printf("\n");
  printf("DigestPlay for BrandMeister DMR Master Server\n");
  printf("Copyright 2017 Artem Prilutskiy (R3ABM, cyanide.burnout@gmail.com)\n");
  printf("Software revision " STRING(VERSION) " build " BUILD "\n");
  printf("\n");

  // Main variables

  uint32_t number = 0;
  const char* port = "54005";
  const char* location = NULL;
  const char* password = NULL;

  struct RewindSuperHeader header;
  memset(&header, 0, sizeof(struct RewindSuperHeader));

  // Start up

  struct option options[] =
  {
    { "client-password",  required_argument, NULL, 'w' },
    { "client-number",    required_argument, NULL, 'c' },
    { "server-address",   required_argument, NULL, 's' },
    { "server-port",      required_argument, NULL, 'p' },
    { "source-id",        required_argument, NULL, 'u' },
    { "group-id",         required_argument, NULL, 'g' },
    { "talker-alias",     required_argument, NULL, 't' },
    { NULL,               0,                 NULL, 0   }
  };

  int value = 0;
  int control = 0;
  int selection = 0;

  while ((selection = getopt_long(argc, argv, "w:c:s:p:u:g:t:", options, NULL)) != EOF)
    switch (selection)
    {
      case 'w':
        password = optarg;
        control |= 0b00001;
        break;

      case 's':
        location = optarg;
        control |= 0b00010;
        break;

      case 'p':
        port = optarg;
        break;

      case 'c':
        number = strtol(optarg, NULL, 10);
        control |= 0b00100;
        break;

      case 'u':
        value = strtol(optarg, NULL, 10);
        if (value > 0)
        {
          header.sourceID = htole32(value);
          control |= 0b01000;
        }
        break;

      case 'g':
        value = strtol(optarg, NULL, 10);
        if (value > 0)
        {
          header.destinationID = htole32(value);
          control |= 0b10000;
        }
        break;

      case 't':
        strncpy(header.sourceCall, optarg, REWIND_CALL_LENGTH);
        break;
    }

  if (control != 0b11111)
  {
    printf(
      "Usage:\n"
      "  %s\n"
      "    --client-number <Registered ID of client>\n"
      "    --client-password <access password for BrandMeister DMR Server>\n"
      "    --server-address <domain name of BrandMeister DMR Server>\n"
      "    --server-port <service port for BrandMeister DMR Server>\n"
      "    --source-id <ID to use as a source>\n"
      "    --group-id <TG ID>\n"
      "    --talker-alias <text to send as Talker Alias>\n"
      "\n",
      argv[0]);
    return EXIT_FAILURE;
  }

  // Create Rewind client context

  struct RewindContext* context = CreateRewindContext(number, CLIENT_NAME);

  if (context == NULL)
  {
    printf("Error creating context\n");
    return EXIT_FAILURE;
  }

  // Check input data format

  char* buffer = (char*)alloca(BUFFER_SIZE);

  if ((read(STDIN_FILENO, buffer, DSD_MAGIC_SIZE) != DSD_MAGIC_SIZE) ||
      (memcmp(buffer, DSD_MAGIC_TEXT, DSD_MAGIC_SIZE) != 0))
  {
    printf("Error checking input data format\n");
    ReleaseRewindContext(context);
    return EXIT_FAILURE;
  }

  // Connect to the server

  int result = ConnectRewindClient(context, location, port, password, REWIND_OPTION_LINEAR_FRAME);

  if (result < 0)
  {
    printf("Cannot connect to the server (%i)\n", result);
    ReleaseRewindContext(context);
    return EXIT_FAILURE;
  }

  // Initialize timer handle

  int handle;
  struct itimerspec interval;

  interval.it_interval.tv_sec  = 0;
  interval.it_interval.tv_nsec = TDMA_FRAME_DURATION * 1000000;

  interval.it_value.tv_sec  = interval.it_interval.tv_sec;
  interval.it_value.tv_nsec = interval.it_interval.tv_nsec;

  handle = timerfd_create(CLOCK_MONOTONIC, 0);
  timerfd_settime(handle, 0, &interval, NULL);

  // Transmit voice header

  printf("Playing...\n");

  header.type = htole32(SESSION_TYPE_GROUP_VOICE);
  TransmitRewindData(context, REWIND_TYPE_SUPER_HEADER, REWIND_FLAG_REAL_TIME_1, &header, sizeof(struct RewindSuperHeader));
  TransmitRewindData(context, REWIND_TYPE_SUPER_HEADER, REWIND_FLAG_REAL_TIME_1, &header, sizeof(struct RewindSuperHeader));
  TransmitRewindData(context, REWIND_TYPE_SUPER_HEADER, REWIND_FLAG_REAL_TIME_1, &header, sizeof(struct RewindSuperHeader));
 
  // Main loop

  uint64_t mark;
  size_t count = 0;

  uint8_t* pointer;
  uint8_t* limit = buffer + 3 * DSD_AMBE_CHUNK_SIZE;

  // Wait for timer event (60 milliseconds)
  while (read(handle, &mark, sizeof(uint64_t)) > 0)
  {
    // Read AMBE chunks in DSD format
 
    pointer = buffer;
    while ((pointer < limit) &&
           (read(STDIN_FILENO, pointer, DSD_AMBE_CHUNK_SIZE) == DSD_AMBE_CHUNK_SIZE))
    {
      pointer += DSD_AMBE_CHUNK_SIZE;
    }

    if (pointer < limit)
    {
      printf("Input data stream ended\n");
      break;
    }

    printf("[> %d <]\r", count);
    fflush(stdout);

    // Convert to linear format

    buffer[0 * DSD_AMBE_CHUNK_SIZE + 7] <<= 7;
    buffer[1 * DSD_AMBE_CHUNK_SIZE + 7] <<= 7;
    buffer[2 * DSD_AMBE_CHUNK_SIZE + 7] <<= 7;

    memmove(buffer + 0 * LINEAR_FRAME_SIZE, buffer + 0 * DSD_AMBE_CHUNK_SIZE + 1, LINEAR_FRAME_SIZE);
    memmove(buffer + 1 * LINEAR_FRAME_SIZE, buffer + 1 * DSD_AMBE_CHUNK_SIZE + 1, LINEAR_FRAME_SIZE);
    memmove(buffer + 2 * LINEAR_FRAME_SIZE, buffer + 2 * DSD_AMBE_CHUNK_SIZE + 1, LINEAR_FRAME_SIZE);

    // Transmit DMR audio frame

    TransmitRewindData(context, REWIND_TYPE_DMR_AUDIO_FRAME, REWIND_FLAG_REAL_TIME_1, buffer, 3 * LINEAR_FRAME_SIZE);

    if ((count % 83) == 0)
    {
      // Every 5 seconds of transmission
      TransmitRewindKeepAlive(context);
    }

    count ++;
  }

  // Transmit call terminator
  TransmitRewindData(context, REWIND_TYPE_DMR_DATA_BASE + 2, REWIND_FLAG_REAL_TIME_1, NULL, 0);

  // Clean up

  TransmitRewindCloae(context);
  ReleaseRewindContext(context);

  printf("Done\n");
  return EXIT_SUCCESS;
};
