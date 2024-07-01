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

#include "GlobalMedia_linux.h"

#include "../Utils.h"
// #include "../Logger.h"
#include "../Error.h"
#include <dbus/dbus.h>
namespace Core::GlobalMedia {

using namespace std::chrono_literals;

void sendDbusCommand(const char* method) {
    DBusError err;
    DBusConnection* conn;
    DBusMessage* msg;
    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {

        // LOG(Error, L"Connection Error: " + std::wstring(err.message));
        dbus_error_free(&err);
    }
    if (conn == nullptr) return;
    msg = dbus_message_new_method_call("org.mpris.MediaPlayer2.player",
                                       "/org/mpris/MediaPlayer2",
                                       "org.mpris.MediaPlayer2.Player",
                                       method);
    if (msg == nullptr) {
        // LOG(Error, L"Message Null");
        exit(1);
    }

    if (!dbus_connection_send(conn, msg, nullptr)) {
        // LOG(Error, L"Out Of Memory!");
        exit(1);
    }
    dbus_connection_flush(conn);
    // LOG(Trace, L"Media paused.");

    dbus_message_unref(msg);

    dbus_connection_unref(conn);
}


void Controller::Play()
{
    sendDbusCommand("Play");
}

void Controller::Pause()
{
    sendDbusCommand("Play");
}
} // namespace Core::GlobalMedia
