// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SpiBus.h"
#include "SpiBusConfig.h"

#include <driver/spi_master.h>

#include <array>
#include <memory>
#include <optional>
#include <utility>

#define SPI_MANAGER_NUM_BUSES SOC_SPI_PERIPH_NUM

class SpiManager {
public:
    explicit SpiManager();
    SpiManager(const SpiManager&) = delete;
    SpiManager& operator=(const SpiManager&) = delete;

#ifdef ARDUINO
    static std::optional<uint8_t> to_arduino(spi_host_device_t host_device);
#endif

    bool register_bus(spi_host_device_t host_device);
    bool claim_bus(spi_host_device_t& host_device);
    // Try to claim the requested specific host. Returns false if the host
    // is not available (not registered or already claimed). No fallback.
    bool claim_bus(spi_host_device_t preferred, spi_host_device_t& host_device);
#ifdef ARDUINO
    std::optional<uint8_t> claim_bus_arduino();
    // Same as above, but returns the Arduino SPI bus number (FSPI/HSPI/VSPI).
    std::optional<uint8_t> claim_bus_arduino(spi_host_device_t preferred);
#endif

    spi_device_handle_t alloc_device(const std::string& bus_id, const std::shared_ptr<SpiBusConfig>& bus_config, spi_device_interface_config_t& device_config, std::optional<spi_host_device_t> preferred_host = std::nullopt);

private:
    std::shared_ptr<SpiBus> get_shared_bus(const std::string& bus_id, std::optional<spi_host_device_t> preferred_host);

    std::array<std::optional<spi_host_device_t>, SPI_MANAGER_NUM_BUSES> available_buses;
    std::array<std::shared_ptr<SpiBus>, SPI_MANAGER_NUM_BUSES> shared_buses;
};

extern SpiManager SpiManagerInst;
