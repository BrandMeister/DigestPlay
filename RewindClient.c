#include "RewindClient.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#ifdef USE_OPENSSL
#include <openssl/sha.h>
#endif

#ifndef HEADER_SHA_H
#include "sha256.h"
#define SHA256_DIGEST_LENGTH  SHA256_BLOCK_SIZE
#define SHA256(data, length, hash) \
  { \
    SHA256_CTX context; \
    sha256_init(&context); \
    sha256_update(&context, data, length); \
    sha256_final(&context, hash); \
  }
#endif

#ifdef __linux__
#include <endian.h>
#include <byteswap.h>
#endif

#ifdef __MACH__
#include <mach/mach.h>
#include <machine/endian.h>

#define htobe16(value)  OSSwapHostToBigInt16(value)
#define be16toh(value)  OSSwapBigToHostInt16(value)
#define htobe32(value)  OSSwapHostToBigInt32(value)
#define be32toh(value)  OSSwapBigToHostInt32(value)

#define htole16(value)  OSSwapHostToLittleInt16(value)
#define le16toh(value)  OSSwapLittleToHostInt16(value)
#define htole32(value)  OSSwapHostToLittleInt32(value)
#define le32toh(value)  OSSwapLittleToHostInt32(value)

#define __bswap_16(value)  OSSwapConstInt16(value)
#define __bswap_32(value)  OSSwapConstInt32(value)
#endif

#define BUFFER_SIZE    256

#define ATTEMPT_COUNT    3
#define RECEIVE_TIMEOUT  2
#define CONNECT_TIMEOUT  5

static int CompareAddresses(struct sockaddr* value1, struct sockaddr_in6* value2)
{
  struct sockaddr_in* value;

  if ((value1->sa_family == AF_INET) &&
      (value2->sin6_family == AF_INET6) &&
      (IN6_IS_ADDR_V4MAPPED(&value2->sin6_addr)))
  {
    value = (struct sockaddr_in*)value1;
    return
      (value->sin_addr.s_addr - value2->sin6_addr.__in6_u.__u6_addr32[3]) |
      (value->sin_port        - value2->sin6_port);
  }

  if ((value1->sa_family == AF_INET6) &&
      (value2->sin6_family == AF_INET6))
  {
    // Compare full socket address
    return memcmp(value1, value2, sizeof(struct sockaddr_in6));
  }

  return -1;
}

struct RewindContext* CreateRewindContext(uint32_t number, const char* verion)
{
  struct utsname name;
  struct timeval interval;
  struct sockaddr_in6 address;
  struct RewindContext* context = (struct RewindContext*)calloc(1, sizeof(struct RewindContext));

  if (context != NULL)
  {
    // Create socket

    address.sin6_family   = AF_INET6;
    address.sin6_addr     = in6addr_any;
    address.sin6_port     = 0;
    address.sin6_scope_id = 0;

    interval.tv_sec  = RECEIVE_TIMEOUT;
    interval.tv_usec = 0;

    context->handle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if ((context->handle < 0) ||
        (bind(context->handle, (struct sockaddr*)&address, sizeof(struct sockaddr_in6)) < 0) ||
        (setsockopt(context->handle, SOL_SOCKET, SO_RCVTIMEO, &interval, sizeof(struct timeval)) < 0))
    {
      close(context->handle);
      free(context);
      return NULL;
    }

    // Create supplementary data

    uname(&name);

    context->data = (struct RewindVersionData*)malloc(BUFFER_SIZE);

    context->length  = sizeof(struct RewindVersionData);
    context->length += sprintf(
      context->data->description,
      "%s %s %s",
      verion,
      name.sysname,
      name.machine);

    context->data->number = htole32(number);
    context->data->service = REWIND_SERVICE_SIMPLE_APPLICATION;
  }

  return context;
}

void ReleaseRewindContext(struct RewindContext* context)
{
  if (context != NULL)
  {
    freeaddrinfo(context->address);
    close(context->handle);
    free(context->data);
    free(context);
  }
}

void TransmitRewindData(struct RewindContext* context, uint16_t type, uint16_t flag, void* data, size_t length)
{
  struct msghdr message;
  struct iovec vectors[2];

  size_t index;
  uint32_t number;

  struct RewindData header;

  memset(&header, 0, sizeof(struct RewindData));
  memcpy(&header, REWIND_PROTOCOL_SIGN, REWIND_SIGN_LENGTH);
 
  index = flag & REWIND_FLAG_REAL_TIME_1;
  number = context->counters[index];

  header.type   = htole16(type);
  header.flags  = htole16(flag);
  header.number = htole32(number);
  header.length = htole16(length);

  vectors[0].iov_base    = &header;
  vectors[0].iov_len     = sizeof(struct RewindData);
  vectors[1].iov_base    = data;
  vectors[1].iov_len     = length;
  message.msg_name       = context->address->ai_addr;
  message.msg_namelen    = context->address->ai_addrlen;
  message.msg_iov        = vectors;
  message.msg_iovlen     = 2;
  message.msg_control    = NULL;
  message.msg_controllen = 0;
  message.msg_flags      = 0;

  sendmsg(context->handle, &message, 0);

  context->counters[index] ++;
}

ssize_t ReceiveRewindData(struct RewindContext* context, struct RewindData* buffer, ssize_t length)
{
  struct sockaddr_in6 address;
  socklen_t size = sizeof(struct sockaddr_in6);

  length = recvfrom(context->handle, buffer, length, 0, (struct sockaddr*)&address, &size);

  if (length < 0)
    return CLIENT_ERROR_SOCKET_IO;

  if (CompareAddresses(context->address->ai_addr, &address) != 0)
    return CLIENT_ERROR_WRONG_ADDRESS;

  if ((length < sizeof(struct RewindData)) ||
      (memcmp(buffer, REWIND_PROTOCOL_SIGN, REWIND_SIGN_LENGTH) != 0))
    return CLIENT_ERROR_WRONG_DATA;

  return length;
}

int ConnectRewindClient(struct RewindContext* context, const char* location, const char* port, const char* password, uint32_t options)
{
  struct addrinfo hints;

  struct RewindData* buffer = (struct RewindData*)alloca(BUFFER_SIZE);
  ssize_t length;

  size_t attempt = 0;
  struct timeval now;
  struct timeval threshold;

  uint8_t* digest = (uint8_t*)alloca(SHA256_DIGEST_LENGTH);

  struct RewindConfigurationData data;

  // Resolve server IP address

  if (context->address != NULL)
  {
    freeaddrinfo(context->address);
    context->address = NULL;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
#ifdef __linux__
  hints.ai_flags  = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
#endif
#ifdef __MACH__
  hints.ai_flags  = AI_V4MAPPED;
  hints.ai_family = AF_INET6;
#endif

  if (getaddrinfo(location, port, &hints, &context->address) != 0)
    return CLIENT_ERROR_DNS_RESOLVE;

  // Do login procedure

  gettimeofday(&now, NULL);
  threshold.tv_sec  = now.tv_sec + CONNECT_TIMEOUT;
  threshold.tv_usec = now.tv_usec; 

  while (timercmp(&now, &threshold, <))
  {
    TransmitRewindData(context, REWIND_TYPE_KEEP_ALIVE, REWIND_FLAG_NONE, context->data, context->length);
    length = ReceiveRewindData(context, buffer, BUFFER_SIZE);

    gettimeofday(&now, NULL);

    if ((length == CLIENT_ERROR_WRONG_ADDRESS) ||
        (length == CLIENT_ERROR_SOCKET_IO) &&
        ((errno == EWOULDBLOCK) ||
         (errno == EAGAIN)))
      continue;

    if (length < 0)
      return length;

    switch (le16toh(buffer->type))
    {
      case REWIND_TYPE_CHALLENGE:
        if (attempt < ATTEMPT_COUNT)
        {
          length -= sizeof(struct RewindData);
          length += sprintf(buffer->data + length, "%s", password);
          SHA256(buffer->data, length, digest);
          TransmitRewindData(context, REWIND_TYPE_AUTHENTICATION, REWIND_FLAG_NONE, digest, SHA256_DIGEST_LENGTH);
          attempt ++;
          continue;
        }
        return CLIENT_ERROR_WRONG_PASSWORD;

      case REWIND_TYPE_KEEP_ALIVE:
        if (options != 0)
        {
          data.options = htole32(options);
          TransmitRewindData(context, REWIND_TYPE_CONFIGURATION, REWIND_FLAG_NONE, &data, sizeof(struct RewindConfigurationData));
          continue;
        }

      case REWIND_TYPE_CONFIGURATION:
        return CLIENT_ERROR_SUCCESS;
    }
  }

  return CLIENT_ERROR_RESPONSE_TIMEOUT;
}

int WaitForRewindSessionEnd(struct RewindContext* context, struct RewindSessionPollData* request, time_t interval1, time_t interval2)
{
  struct RewindData* buffer = (struct RewindData*)alloca(BUFFER_SIZE);
  struct RewindSessionPollData* response = (struct RewindSessionPollData*)buffer->data;
  ssize_t length;

  uint32_t state = 0b00;
  struct timeval now;
  struct timeval threshold1;
  struct timeval threshold2;

  if (interval1 < RECEIVE_TIMEOUT)
    interval1 = RECEIVE_TIMEOUT;

  gettimeofday(&now, NULL);

  threshold1.tv_sec  = now.tv_sec + interval1 + interval2;
  threshold1.tv_usec = now.tv_usec; 

  threshold2.tv_sec  = 0;
  threshold2.tv_usec = 0;

  while (timercmp(&now, &threshold1, <))
  {
    TransmitRewindData(context, REWIND_TYPE_KEEP_ALIVE, REWIND_FLAG_NONE, context->data, context->length);
    TransmitRewindData(context, REWIND_TYPE_SESSION_POLL, REWIND_FLAG_NONE, request, sizeof(struct RewindSessionPollData));

    length = ReceiveRewindData(context, buffer, BUFFER_SIZE);

    gettimeofday(&now, NULL);

    if ((length == CLIENT_ERROR_WRONG_ADDRESS) ||
        (length == CLIENT_ERROR_SOCKET_IO) &&
        ((errno == EWOULDBLOCK) ||
         (errno == EAGAIN)))
      continue;

    if (length < 0)
      return length;

    switch (le16toh(buffer->type))
    {
      case REWIND_TYPE_KEEP_ALIVE:
        state |= 0b01;
        break;

      case REWIND_TYPE_SESSION_POLL:
        if ((response->state   == 0) &&
            (threshold2.tv_sec == 0))
        {
          threshold2.tv_sec  = now.tv_sec + interval2;
          threshold2.tv_usec = now.tv_usec;
        }
        if ((response->state   != 0) &&
            (threshold2.tv_sec != 0))
        {
          threshold2.tv_sec  = 0;
          threshold2.tv_usec = 0;
        }
        if ((threshold2.tv_sec != 0) &&
            (timercmp(&now, &threshold2, >)))
        {
          // No active sessions during <interval2>
          return CLIENT_ERROR_SUCCESS;
        }
        state |= 0b10;
        break;
    }

    if (state == 0b11)
    {
      // Got REWIND_TYPE_KEEP_ALIVE and REWIND_TYPE_SESSION_POLL
      // Wait for 2 seconds before the next attempt
      sleep(RECEIVE_TIMEOUT);
      state = 0b00;
    }
  }

  return CLIENT_ERROR_RESPONSE_TIMEOUT;
}
