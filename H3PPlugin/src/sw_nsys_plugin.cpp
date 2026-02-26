#include <getopt.h>
#include <iostream>
#include <map>
#include <nvtx3/nvToolsExtCounters.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "h3ppci.h"

#define LOG_ERR(...)                                                           \
  fprintf(stderr, __VA_ARGS__);                                                \
  fputs("\n", stderr)

struct Config {
  int deviceIdx = -1;                // -1 means ALL
  std::vector<int> portIndices;      // Empty means ALL
  std::string module = "throughput"; // throughput | error
  int intervalMs = 100;
};

struct MonitoredPort {
  h3ppciDevice_t dev;
  int deviceIdx;
  int portIndex; // Index used in API calls
  int portId;    // Logical Port ID for display
  nvtxDomainHandle_t domain;
  uint64_t counter;
  std::string deviceName;
};

void PrintHelp(const char *progName) {
  printf("Usage: %s [options]\n", progName);
  printf("  -i <idx>      Device index (default: all)\n");
  printf("  -p <p1,p2,..> Port Indices (comma separated, default: all)\n");
  printf("  -m <module>   Module: throughput | error (default: throughput)\n");
  printf("  -t <ms>       Interval in milliseconds (default: 100)\n");
  printf("  -h            Print this help message\n");
}

Config ParseArgs(int argc, char **argv) {
  Config config;
  int opt;
  while ((opt = getopt(argc, argv, "i:p:m:t:h")) != -1) {
    switch (opt) {
    case 'i':
      config.deviceIdx = atoi(optarg);
      break;
    case 'p': {
      // Handle the immediate argument (e.g., -p 0,32)
      char *ptr = strtok(optarg, ",");
      while (ptr != nullptr) {
        config.portIndices.push_back(atoi(ptr));
        ptr = strtok(nullptr, ",");
      }
      // Handle subsequent arguments that aren't options (e.g., -p 0 32)
      while (optind < argc && argv[optind][0] != '-') {
        char *extraPtr = strtok(argv[optind], ",");
        while (extraPtr != nullptr) {
          config.portIndices.push_back(atoi(extraPtr));
          extraPtr = strtok(nullptr, ",");
        }
        optind++;
      }
      break;
    }
    case 'm':
      config.module = optarg;
      break;
    case 't':
      config.intervalMs = atoi(optarg);
      break;
    case 'h':
      PrintHelp(argv[0]);
      exit(0);
    default:
      PrintHelp(argv[0]);
      exit(1);
    }
  }
  return config;
}

int main(int argc, char **argv) {
  Config config = ParseArgs(argc, argv);

  if (config.module != "throughput" && config.module != "error") {
    LOG_ERR("Invalid module: %s. Must be 'throughput' or 'error'.",
            config.module.c_str());
    return 1;
  }

  int totalDevices = 0;
  if (h3ppciGetDeviceCount(&totalDevices) != H3PPCI_SUCCESS ||
      totalDevices == 0) {
    LOG_ERR("No H3P devices found.");
    return 1;
  }

  std::vector<MonitoredPort> monitoredPorts;
  std::vector<h3ppciDevice_t> activeDevices;
  std::map<int, nvtxDomainHandle_t> deviceDomains;

  for (int d = 0; d < totalDevices; ++d) {
    if (config.deviceIdx != -1 && config.deviceIdx != d)
      continue;

    h3ppciDevice_t dev;
    if (h3ppciGetDevice(&dev, d) != H3PPCI_SUCCESS)
      continue;

    h3ppciDeviceProp prop;
    h3ppciGetDeviceProperties(&prop, dev);

    char bdfStr[32];
    snprintf(bdfStr, sizeof(bdfStr), "%04x:%02x:%02x.%x", prop.domain, prop.bus,
             prop.device, prop.function);

    std::string domainName = std::string("H3P_PCIe_Switch/") + prop.name + "_" +
                             std::to_string(d) + "(" + bdfStr + ")";
    nvtxDomainHandle_t domain = nvtxDomainCreateA(domainName.c_str());
    deviceDomains[d] = domain;
    activeDevices.push_back(dev);

    int portCount = 0;
    h3ppciGetPortCount(dev, &portCount);

    // Setup NVTX Schema for this domain
    std::vector<nvtxPayloadSchemaEntry_t> schemaIndices;
    std::vector<std::string> metricNames;
    if (config.module == "throughput") {
      metricNames = {"RX_MBs", "TX_MBs", "RX_Util", "TX_Util"};
    } else {
      metricNames = {"BadTLP", "BadDLLP", "RxErr", "RecDiag"};
    }

    for (const auto &name : metricNames) {
      schemaIndices.push_back({0, NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE, name.c_str(),
                               "", 0, 0, nullptr, nullptr});
    }

    nvtxPayloadSchemaAttr_t schemaAttr;
    memset(&schemaAttr, 0, sizeof(schemaAttr));
    schemaAttr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                           NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                           NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
                           NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
    schemaAttr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
    schemaAttr.entries = schemaIndices.data();
    schemaAttr.numEntries = schemaIndices.size();
    schemaAttr.payloadStaticSize = schemaIndices.size() * sizeof(double);

    const uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &schemaAttr);

    for (int p = 0; p < portCount; p++) {
      if (!config.portIndices.empty()) {
        bool found = false;
        for (int pi : config.portIndices) {
          if (pi == p) {
            found = true;
            break;
          }
        }
        if (!found)
          continue;
      }

      h3ppciPortInfo portInfo;
      if (h3ppciGetPortInfo(dev, p, &portInfo) != H3PPCI_SUCCESS)
        continue;

      std::string counterName = std::string("Port_") +
                                std::to_string(portInfo.portId) + "_" +
                                config.module;
      nvtxCounterAttr_t cntAttr = {};
      cntAttr.structSize = sizeof(nvtxCounterAttr_t);
      cntAttr.schemaId = schemaId;
      cntAttr.name = counterName.c_str();
      cntAttr.scopeId = NVTX_SCOPE_CURRENT_VM;
      uint64_t counter = nvtxCounterRegister(domain, &cntAttr);

      monitoredPorts.push_back(
          {dev, d, p, portInfo.portId, domain, counter, prop.name});
    }

    if (config.module == "throughput") {
      h3ppciInitDevice(dev);
    }
  }

  if (monitoredPorts.empty()) {
    LOG_ERR("No ports matched criteria.");
    return 1;
  }

  printf(
      "Monitoring %zu ports across %zu devices. Module: %s, Interval: %d ms\n",
      monitoredPorts.size(), activeDevices.size(), config.module.c_str(),
      config.intervalMs);
  printf("Press Ctrl+C to stop.\n");

  std::vector<double> values(4); // Fixed to 4 metrics for both modules

  while (true) {
    if (config.module == "throughput") {
      for (auto dev_h : activeDevices)
        h3ppciPerfStart(dev_h);
      usleep(config.intervalMs * 1000);
      for (auto dev_h : activeDevices)
        h3ppciPerfStop(dev_h);

      for (auto &mp : monitoredPorts) {
        h3ppciPerfCal cal;
        if (h3ppciPerfGetCal(mp.dev, mp.portIndex, &cal) == H3PPCI_SUCCESS) {
          values[0] = cal.rxBps / (1024.0 * 1024.0);
          values[1] = cal.txBps / (1024.0 * 1024.0);
          values[2] = cal.rxUtilization;
          values[3] = cal.txUtilization;
          nvtxCounterSample(mp.domain, mp.counter, values.data(),
                            4 * sizeof(double));
        }
      }
    } else {
      usleep(config.intervalMs * 1000);
      for (auto &mp : monitoredPorts) {
        h3ppciPortErrors errs;
        if (h3ppciGetPortErrorCounters(mp.dev, mp.portIndex, &errs) ==
            H3PPCI_SUCCESS) {
          values[0] = (double)errs.badTlp;
          values[1] = (double)errs.badDllp;
          values[2] = (double)errs.rxErrors;
          values[3] = (double)errs.recoveryDiagnostics;
          nvtxCounterSample(mp.domain, mp.counter, values.data(),
                            4 * sizeof(double));
        }
      }
    }

    if (monitoredPorts.size() == 1) {
      printf("\rSampled Port %d: %.2f %.2f %.2f %.2f          ",
             monitoredPorts[0].portId, values[0], values[1], values[2],
             values[3]);
      fflush(stdout);
    } else {
      static int iterations = 0;
      printf("\rSampling %zu ports... [Iter: %d]          ",
             monitoredPorts.size(), ++iterations);
      fflush(stdout);
    }
  }

  return 0;
}
