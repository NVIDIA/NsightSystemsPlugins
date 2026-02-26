#ifndef H3PPCI_H
#define H3PPCI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum h3ppciError_t {
  H3PPCI_SUCCESS = 0,
  H3PPCI_ERROR_NOT_INITIALIZED = 1,
  H3PPCI_ERROR_INVALID_DEVICE = 2,
  H3PPCI_ERROR_INVALID_PORT = 3,
  H3PPCI_ERROR_MEMORY_ERROR = 4,
  H3PPCI_ERROR_FILE_ERROR = 5,
  H3PPCI_ERROR_UNSUPPORTED = 6,
  H3PPCI_ERROR_SEQUENCE = 7,
  H3PPCI_ERROR_UNKNOWN = 99
} h3ppciError_t;

typedef int h3ppciDevice_t;

typedef struct h3ppciDeviceProp {
  char name[256];
  int domain;
  int bus;
  int device;
  int function;
  unsigned short vendorId;
  unsigned short deviceId;
  unsigned short revisionId;
  char serialNumber[64];
} h3ppciDeviceProp;

typedef struct h3ppciLinkState {
  int gen;           // e.g., 3, 4, 5
  int width;         // e.g., 1, 4, 8, 16
  char speedStr[16]; // "16.0 GT/s"
} h3ppciLinkState;

typedef struct h3ppciAttachedDevice {
  char bdf[16];
  unsigned short vendorId;
  unsigned short deviceId;
  unsigned short subVendorId;
  unsigned short subDeviceId;
  int mps;
  int mpss;
  int mrr;
  h3ppciLinkState curLink;
  h3ppciLinkState maxLink;
} h3ppciAttachedDevice;

typedef struct h3ppciPortInfo {
  int portId;     // Logical/Global port ID
  int stationId;  // Internal station ID
  int portNum;    // Internal port number
  int isUpstream; // 1 if upstream, 0 if downstream
  int isHost;
  int isFabric;
  int enabled; // 1 if port is enabled/active, 0 otherwise

  char bdf[16]; // PCI BDF string, e.g., "0000:01:00.0"
  int mrr;      // Max Read Request Size (bytes)
  int mps;      // Max Payload Size (bytes)
  int mpss;     // Max Payload Size Supported (bytes)

  h3ppciLinkState maxLink;
  h3ppciLinkState curLink;
} h3ppciPortInfo;

typedef struct h3ppciPortErrors {
  unsigned long long badTlp;
  unsigned long long badDllp;
  unsigned long long rxErrors;
  unsigned long long recoveryDiagnostics;
} h3ppciPortErrors;

typedef struct h3ppciPortThroughput {
  unsigned long long rxBytes;
  unsigned long long txBytes;
} h3ppciPortThroughput;

typedef struct h3ppciPerfCal {
  unsigned long long intervalMs;
  unsigned long long rxBytes;
  unsigned long long txBytes;
  double rxBps;
  double txBps;
  double rxUtilization; // 0.0 - 100.0%
  double txUtilization; // 0.0 - 100.0%
} h3ppciPerfCal;

// Initialization / Device Enumeration
h3ppciError_t h3ppciGetDeviceCount(int *count);
h3ppciError_t h3ppciGetDevice(h3ppciDevice_t *device, int index);
h3ppciError_t h3ppciGetDeviceProperties(h3ppciDeviceProp *prop,
                                        h3ppciDevice_t device);

// Port Enumeration (Specific to PCIe Switch)
h3ppciError_t h3ppciGetPortCount(h3ppciDevice_t device, int *count);
h3ppciError_t h3ppciGetPortInfo(h3ppciDevice_t device, int portIndex,
                                h3ppciPortInfo *info);

h3ppciError_t h3ppciGetAttachedDevice(h3ppciDevice_t device, int portIndex,
                                      h3ppciAttachedDevice *attached);

// Error Counters
h3ppciError_t h3ppciGetPortErrorCounters(h3ppciDevice_t device, int portIndex,
                                         h3ppciPortErrors *errors);

// Performance / Throughput
// Usage: InitDevice -> PerfStart(All Ports) -> Sleep -> PerfStop(All Ports) ->
// PerfGet(Per Port)
h3ppciError_t h3ppciInitDevice(h3ppciDevice_t device);

// Start performance monitoring for ALL ports on the device
h3ppciError_t h3ppciPerfStart(h3ppciDevice_t device);

// Stop performance monitoring for ALL ports on the device
h3ppciError_t h3ppciPerfStop(h3ppciDevice_t device);

// Get raw throughput data for a specific port (uses internal timer for
// interval)
h3ppciError_t h3ppciPerfGet(h3ppciDevice_t device, int portIndex,
                            h3ppciPortThroughput *throughput);

// Get the interval (in ms) between the last PerfStart and PerfStop
h3ppciError_t h3ppciGetPerfInterval(h3ppciDevice_t device,
                                    unsigned long long *intervalMs);

// Unified API: Gets Interval, Bytes, Bps, and calculates Utilization
// automatically
h3ppciError_t h3ppciPerfGetCal(h3ppciDevice_t device, int portIndex,
                               h3ppciPerfCal *cal);

// Helper Utilities
h3ppciError_t h3ppciCalculateBps(unsigned long long bytes,
                                 unsigned long long intervalMs, double *bps);

// Latency Measurement
typedef struct h3ppciLatency {
  unsigned int trtMin;
  unsigned int trtMax;
  unsigned int ackMax;
  int isActive;
} h3ppciLatency;

h3ppciError_t h3ppciResetLatency(h3ppciDevice_t device, int portIndex);
h3ppciError_t h3ppciGetLatency(h3ppciDevice_t device, int portIndex,
                               h3ppciLatency *latency);

// Calculate Link Utilization based on Gen, Width and actual Bytes
h3ppciError_t h3ppciCalculateUtilization(unsigned long long bytes,
                                         unsigned long long intervalMs, int gen,
                                         int width, double *utilizationPct);

// Error Handling
const char *h3ppciGetErrorString(h3ppciError_t error);

#ifdef __cplusplus
}
#endif

#endif // H3PPCI_H
