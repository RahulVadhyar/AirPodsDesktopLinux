//
// AirPodsDesktop - AirPods Desktop User Experience Enhancement Program.
// Copyright (C) 2021-2022 SpriteOvO
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#if !defined APD_OS_LINUX
    #error "This file shouldn't be compiled."
#endif

#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <dbus/dbus.h>
#include <sdbus-c++/sdbus-c++.h>

#include "Bluetooth_abstract.h"

namespace Core::Bluetooth {

class Device final : public Details::DeviceAbstract<std::string>
{
public:
    Device(const std::string &path);
    Device(const Device &rhs);
    Device(Device &&rhs) noexcept;
    ~Device();

    Device &operator=(const Device &rhs);
    Device &operator=(Device &&rhs) noexcept;

    std::string GetAddress() const override;
    std::string GetName() const override;
    uint16_t GetVendorId() const override;
    uint16_t GetProductId() const override;
    DeviceState GetConnectionState() const override;

private:
    std::string _path;
    std::string _address;
    std::string _name;
    uint16_t _vendorId;
    uint16_t _productId;
    DeviceState _connectionState;

    void CopyFrom(const Device &rhs);
    void MoveFrom(Device &&rhs) noexcept;

    void FetchProperties();
};

namespace DeviceManager {

std::vector<Device> GetDevicesByState(DeviceState state);
std::optional<Device> FindDevice(const std::string &address);

} // namespace DeviceManager

class AdvertisementWatcher final
    : public Details::AdvertisementWatcherAbstract<AdvertisementWatcher>
{
public:
    using Timestamp = std::chrono::system_clock::time_point;

    AdvertisementWatcher();
    ~AdvertisementWatcher();

    bool Start() override;
    bool Stop() override;

private:
    static constexpr std::chrono::seconds kRetryInterval{3};
    
    std::atomic<bool> _stop{false}, _destroy{false};
    std::atomic<std::chrono::steady_clock::time_point> _lastStartTime;
    std::mutex _conVarMutex;
    std::condition_variable _stopConVar, _destroyConVar;

    std::unique_ptr<sdbus::IConnection> _connection;
    std::unique_ptr<sdbus::IObject> _object;

    void OnReceived(const sdbus::Signal &signal);
    void OnStopped(const sdbus::Signal &signal);
};
} // namespace Core::Bluetooth
