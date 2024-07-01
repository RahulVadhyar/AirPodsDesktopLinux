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

#include "Bluetooth_linux.h"

// #include "../Logger.h"
#include "Debug.h"
#include "OS/linux.h"

namespace Core::Bluetooth {

//////////////////////////////////////////////////
// Device
//

Device::Device(const std::string &path) : _path(path)
{
    FetchProperties();
}

Device::Device(const Device &rhs)
{
    CopyFrom(rhs);
}

Device::Device(Device &&rhs) noexcept
{
    MoveFrom(std::move(rhs));
}

Device::~Device()
{
    // Clean up resources if any
}

Device &Device::operator=(const Device &rhs)
{
    CopyFrom(rhs);
    return *this;
}

Device &Device::operator=(Device &&rhs) noexcept
{
    MoveFrom(std::move(rhs));
    return *this;
}

std::string Device::GetAddress() const
{
    return _address;
}

std::string Device::GetName() const
{
    return _name;
}

uint16_t Device::GetVendorId() const
{
    return _vendorId;
}

uint16_t Device::GetProductId() const
{
    return _productId;
}

DeviceState Device::GetConnectionState() const
{
    return _connectionState;
}

void Device::CopyFrom(const Device &rhs)
{
    _path = rhs._path;
    _address = rhs._address;
    _name = rhs._name;
    _vendorId = rhs._vendorId;
    _productId = rhs._productId;
    _connectionState = rhs._connectionState;
}

void Device::MoveFrom(Device &&rhs) noexcept
{
    _path = std::move(rhs._path);
    _address = std::move(rhs._address);
    _name = std::move(rhs._name);
    _vendorId = rhs._vendorId;
    _productId = rhs._productId;
    _connectionState = rhs._connectionState;
}

void Device::FetchProperties()
{
    // Using sdbus-c++ to interact with the BlueZ D-Bus API to fetch properties
    auto connection = sdbus::createSystemBusConnection();
    auto proxy = sdbus::createProxy(*connection, "org.bluez", _path);
    proxy->callMethod("GetAll").onInterface("org.freedesktop.DBus.Properties")
        .withArguments("org.bluez.Device1")
        .storeResultsTo(_properties);

    _address = _properties["Address"].get();
    _name = _properties["Name"].get();
    _vendorId = _properties["VendorID"].get();
    _productId = _properties["ProductID"].get();
    auto connected = _properties["Connected"].get();
    _connectionState = connected ? DeviceState::Connected : DeviceState::Disconnected;
}

//////////////////////////////////////////////////
// DevicesManager
//

namespace Details {
class DeviceManager final : public Helper::Singleton<DeviceManager>,
                            Details::DeviceManagerAbstract<Device>
{
protected:
    DeviceManager() = default;
    friend Helper::Singleton<DeviceManager>;

public:
    std::vector<Device> GetDevicesByState(DeviceState state) const override
    {
        std::vector<Device> result;
        auto connection = sdbus::createSystemBusConnection();
        auto proxy = sdbus::createProxy(*connection, "org.bluez", "/");
        sdbus::MethodCall method = proxy->createMethodCall("org.bluez.Adapter1", "GetManagedObjects");
        sdbus::Variant managedObjects;
        proxy->callMethod(method).storeResultsTo(managedObjects);

        auto objects = managedObjects.get<std::map<sdbus::ObjectPath, std::map<std::string, sdbus::Variant>>>();
        for (const auto &object : objects)
        {
            if (object.second.count("org.bluez.Device1"))
            {
                Device device(object.first);
                if ((state == DeviceState::Connected && device.GetConnectionState() == DeviceState::Connected) ||
                    (state == DeviceState::Disconnected && device.GetConnectionState() == DeviceState::Disconnected))
                {
                    result.push_back(std::move(device));
                }
            }
        }

        return result;
    }

    std::optional<Device> FindDevice(const std::string &address) const override
    {
        auto devices = GetDevicesByState(DeviceState::Connected);
        for (const auto &device : devices) {
            if (device.GetAddress() == address) {
                return device;
            }
        }
        return std::nullopt;
    }
};
} // namespace Details

namespace DeviceManager {

std::vector<Device> GetDevicesByState(DeviceState state)
{
    return Details::DeviceManager::GetInstance().GetDevicesByState(state);
}

std::optional<Device> FindDevice(const std::string &address)
{
    return Details::DeviceManager::GetInstance().FindDevice(address);
}
} // namespace DeviceManager

//////////////////////////////////////////////////
// AdvertisementWatcher
//

AdvertisementWatcher::AdvertisementWatcher()
{
    _connection = sdbus::createSystemBusConnection();
    _object = sdbus::createObject(*_connection, "/org/bluez");
}

AdvertisementWatcher::~AdvertisementWatcher()
{
    if (!_stop) {
        _destroy = true;
        Stop();
        std::unique_lock<std::mutex> lock{_conVarMutex};
        _destroyConVar.wait_for(lock, 1s);
    }
}

bool AdvertisementWatcher::Start()
{
    try {
        _stop = false;
        _lastStartTime = std::chrono::steady_clock::now();

        _object->registerSignal("org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
            [this](sdbus::Signal &signal) { this->OnReceived(signal); });
        _object->registerSignal("org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
            [this](sdbus::Signal &signal) { this->OnStopped(signal); });
        
        return true;
    } catch (const std::exception &e) {
        // Logger::LogError("Failed to start AdvertisementWatcher: ", e.what());
        return false;
    }
}

bool AdvertisementWatcher::Stop()
{
    try {
        _stop = true;
        _object->unregisterSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesAdded");
        _object->unregisterSignalHandler("org.freedesktop.DBus.ObjectManager", "InterfacesRemoved");

        std::unique_lock<std::mutex> lock{_conVarMutex};
        _stopConVar.wait_for(lock, kRetryInterval);

        return true;
    } catch (const std::exception &e) {
        // Logger::LogError("Failed to stop AdvertisementWatcher: ", e.what());
        return false;
    }
}

void AdvertisementWatcher::OnReceived(const sdbus::Signal &signal)
{
    // Process the signal and notify the callback
    std::string path;
    signal >> path;

    // Assuming signal contains device information
    if (_stop) return;

    // Notify the callback
    if (_callback) {
        _callback(path);
    }
}

void AdvertisementWatcher::OnStopped(const sdbus::Signal &signal)
{
    // Process the stop signal
    if (_stop) {
        std::unique_lock<std::mutex> lock{_conVarMutex};
        _stopConVar.notify_all();
    }
    if (_destroy) {
        std::unique_lock<std::mutex> lock{_conVarMutex};
        _destroyConVar.notify_all();
    }
}

} // namespace Core::Bluetooth
