#ifndef REWINDCLIENT_H
#define REWINDCLIENT_H

#include <stddef.h>
#include <stdint.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include "Rewind.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SESSION_TYPE_PRIVATE_VOICE    5
#define SESSION_TYPE_GROUP_VOICE      7

#define CLIENT_ERROR_SUCCESS           0
#define CLIENT_ERROR_SOCKET_IO         -1
#define CLIENT_ERROR_WRONG_ADDRESS     -2
#define CLIENT_ERROR_WRONG_DATA        -3
#define CLIENT_ERROR_DNS_RESOLVE       -4
#define CLIENT_ERROR_ATTEMPT_EXCEEDED  -5
#define CLIENT_ERROR_RESPONSE_TIMEOUT  -6

struct RewindContext
{
  int handle;
  struct addrinfo* address;

  uint32_t counters[2];

  struct RewindVersionData* data;
  size_t length;
};

struct RewindContext* CreateRewindContext(uint32_t number, const char* verion);
void ReleaseRewindContext(struct RewindContext* context);

void TransmitRewindData(struct RewindContext* context, uint16_t type, uint16_t flag, void* data, size_t length);
ssize_t ReceiveRewindData(struct RewindContext* context, struct RewindData* buffer, ssize_t length);

int ConnectRewindClient(struct RewindContext* context, const char* location, const char* port, const char* password, uint32_t options);

#define TransmitRewindKeepAlive(context)   TransmitRewindData(context, REWIND_TYPE_KEEP_ALIVE, REWIND_FLAG_NONE, context->data, context->length);
#define TransmitRewindCloae(context)       TransmitRewindData(context, REWIND_TYPE_CLOSE,      REWIND_FLAG_NONE, NULL,          0              );

#ifdef __cplusplus
}
#endif

#endif