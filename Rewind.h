#ifndef REWIND_H
#define REWIND_H

#include <stdint.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push, 1)

#define REWIND_KEEP_ALIVE_INTERVAL   5

#define REWIND_SIGN_LENGTH           8
#define REWIND_PROTOCOL_SIGN         "REWIND01"

#define REWIND_CLASS_REWIND_CONTROL  0x0000
#define REWIND_CLASS_SYSTEM_CONSOLE  0x0100
#define REWIND_CLASS_SERVER_NOTICE   0x0200
#define REWIND_CLASS_DEVICE_DATA     0x0800
#define REWIND_CLASS_APPLICATION     0x0900
#define REWIND_CLASS_TERMINAL        0x0a00

#define REWIND_CLASS_KAIROS_DATA       (REWIND_CLASS_DEVICE_DATA + 0x00)
#define REWIND_CLASS_HYTERA_DATA       (REWIND_CLASS_DEVICE_DATA + 0x10)

#define REWIND_TYPE_KEEP_ALIVE         (REWIND_CLASS_REWIND_CONTROL + 0)
#define REWIND_TYPE_CLOSE              (REWIND_CLASS_REWIND_CONTROL + 1)
#define REWIND_TYPE_CHALLENGE          (REWIND_CLASS_REWIND_CONTROL + 2)
#define REWIND_TYPE_AUTHENTICATION     (REWIND_CLASS_REWIND_CONTROL + 3)

#define REWIND_TYPE_REDIRECTION        (REWIND_CLASS_REWIND_CONTROL + 8)

#define REWIND_TYPE_REPORT             (REWIND_CLASS_SYSTEM_CONSOLE + 0)

#define REWIND_TYPE_BUSY_NOTICE        (REWIND_CLASS_SERVER_NOTICE + 0)
#define REWIND_TYPE_ADDRESS_NOTICE     (REWIND_CLASS_SERVER_NOTICE + 1)
#define REWIND_TYPE_BINDING_NOTICE     (REWIND_CLASS_SERVER_NOTICE + 2)

#define REWIND_TYPE_EXTERNAL_SERVER    (REWIND_CLASS_KAIROS_DATA + 0)
#define REWIND_TYPE_REMOTE_CONTROL     (REWIND_CLASS_KAIROS_DATA + 1)
#define REWIND_TYPE_SNMP_TRAP          (REWIND_CLASS_KAIROS_DATA + 2)

#define REWIND_TYPE_PEER_DATA          (REWIND_CLASS_HYTERA_DATA + 0)
#define REWIND_TYPE_RDAC_DATA          (REWIND_CLASS_HYTERA_DATA + 1)
#define REWIND_TYPE_MEDIA_DATA         (REWIND_CLASS_HYTERA_DATA + 2)

#define REWIND_TYPE_CONFIGURATION      (REWIND_CLASS_APPLICATION + 0x00)
#define REWIND_TYPE_SUBSCRIPTION       (REWIND_CLASS_APPLICATION + 0x01)
#define REWIND_TYPE_CANCELLING         (REWIND_CLASS_APPLICATION + 0x02)
#define REWIND_TYPE_SESSION_POLL       (REWIND_CLASS_APPLICATION + 0x03)
#define REWIND_TYPE_DMR_DATA_BASE      (REWIND_CLASS_APPLICATION + 0x10)
#define REWIND_TYPE_DMR_AUDIO_FRAME    (REWIND_CLASS_APPLICATION + 0x20)
#define REWIND_TYPE_DMR_EMBEDDED_DATA  (REWIND_CLASS_APPLICATION + 0x27)
#define REWIND_TYPE_SUPER_HEADER       (REWIND_CLASS_APPLICATION + 0x28)
#define REWIND_TYPE_FAILURE_CODE       (REWIND_CLASS_APPLICATION + 0x29)

#define REWIND_TYPE_TERMINAL_IDLE      (REWIND_CLASS_TERMINAL + 0x00)
#define REWIND_TYPE_TERMINAL_ATTACH    (REWIND_CLASS_TERMINAL + 0x02)
#define REWIND_TYPE_TERMINAL_DETACH    (REWIND_CLASS_TERMINAL + 0x03)
#define REWIND_TYPE_MESSAGE_TEXT       (REWIND_CLASS_TERMINAL + 0x10)
#define REWIND_TYPE_MESSAGE_STATUS     (REWIND_CLASS_TERMINAL + 0x11)
#define REWIND_TYPE_LOCATION_REPORT    (REWIND_CLASS_TERMINAL + 0x20)
#define REWIND_TYPE_LOCATION_REQUEST   (REWIND_CLASS_TERMINAL + 0x21)

#define REWIND_FLAG_NONE             0
#define REWIND_FLAG_REAL_TIME_1      (1 << 0)
#define REWIND_FLAG_REAL_TIME_2      (1 << 1)
#define REWIND_FLAG_BUFFERING        (1 << 2)
#define REWIND_FLAG_DEFAULT_SET      REWIND_FLAG_NONE

#define REWIND_ROLE_REPEATER_AGENT   0x10
#define REWIND_ROLE_APPLICATION      0x20

#define REWIND_SERVICE_CRONOS_AGENT        (REWIND_ROLE_REPEATER_AGENT + 0)
#define REWIND_SERVICE_TELLUS_AGENT        (REWIND_ROLE_REPEATER_AGENT + 1)
#define REWIND_SERVICE_SIMPLE_APPLICATION  (REWIND_ROLE_APPLICATION    + 0)
#define REWIND_SERVICE_OPEN_TERMINAL       (REWIND_ROLE_APPLICATION    + 1)

#define REWIND_OPTION_SUPER_HEADER  (1 << 0)
#define REWIND_OPTION_LINEAR_FRAME  (1 << 1)

#define REWIND_CALL_LENGTH  10

// Keep-Alive

struct RewindVersionData
{
  uint32_t number;      // Remote ID
  uint8_t service;      // REWIND_SERVICE_*
  char description[0];  // Software name and version
};

// Redirection

union RewindAddress
{
  struct in_addr v4;
  struct in6_addr v6;
};

struct RewindRedirectionData
{
  uint16_t family;              // Address family: AF_INET, AF_INET6 or AF_UNSPEC
  uint16_t port;                // UDP port
  union RewindAddress address;  // 
};

// Generic Data Structures

struct RewindAddressData
{
  struct in_addr address;
  uint16_t port;
};

struct RewindBindingData
{
  uint16_t ports[0];
};

// Simple Application Protocol

struct RewindConfigurationData
{
  uint32_t options;  // REWIND_OPTION_*
};

struct RewindSubscriptionData
{
  uint32_t type;    // SESSION_TYPE_*
  uint32_t number;  // Destination ID
};

struct RewindSessionPollData
{
  uint32_t type;    // TREE_SESSION_*
  uint32_t flag;    // SESSION_FLAG_*
  uint32_t number;  // ID
  uint32_t state;   // 
};

struct RewindSuperHeader
{
  uint32_t type;                             // SESSION_TYPE_*
  uint32_t sourceID;                         // Source ID or 0
  uint32_t destinationID;                    // Destination ID or 0
  char sourceCall[REWIND_CALL_LENGTH];       // Source Call or zeros
  char destinationCall[REWIND_CALL_LENGTH];  // Destination Call or zeros
};

// Open DMR Terminal

struct RewindTextMessageData
{
  uint32_t reserved;       // Reserved for future use, should be 0
  uint32_t sourceID;       // Source ID
  uint32_t destinationID;  // Destination ID
  uint16_t option;         // CHEAD_GROUP_DESTINATION = 128, private message = 0
  uint16_t length;         // Length of message in bytes
  uint16_t data[0];        // Message text, UTF-16LE
};

struct RewindTextMessageStatus
{
  uint32_t reserved;       // Reserved for future use, should be 0
  uint32_t sourceID;       // Source ID
  uint32_t destinationID;  // Destination ID
  uint8_t status;          // STATUS_TYPE_*, corresponds to status field of DMR response header of data call
};

struct RewindLocationRequest
{
  uint32_t reserved;       // Reserved for future use, should be 0
  uint32_t type;           // LOCATION_REQUEST_SHOT = 0, LOCATION_REQUEST_TIMED_START = 1, LOCATION_REQUEST_TIMED_STOP = 2
  uint32_t interval;       // Interval of timed report in seconds
};

struct RewindLocationReport
{
  uint32_t reserved;       // Reserved for future use, should be 0
  uint32_t format;         // LOCATION_FORMAT_NMEA = 0
  uint16_t length;         // Length of message in bytes
  char data[0];            // NMEA position data
};

// Rewind Transport Layer

struct RewindData
{
  char sign[REWIND_SIGN_LENGTH];
  uint16_t type;    // REWIND_TYPE_*
  uint16_t flags;   // REWIND_FLAG_*
  uint32_t number;  // Packet sequence number
  uint16_t length;  // Length of following data
  uint8_t data[0];  //
};

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
