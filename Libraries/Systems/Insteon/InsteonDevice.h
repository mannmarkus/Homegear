/* Copyright 2013-2014 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef INSTEONDEVICE_H
#define INSTEONDEVICE_H

#include "../../LogicalDevices/LogicalDevice.h"
#include "../../HelperFunctions/HelperFunctions.h"
#include "InsteonDeviceTypes.h"
#include "InsteonPacket.h"

#include <string>
#include <unordered_map>
#include <map>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <chrono>
#include "pthread.h"

namespace Insteon
{
class InsteonDevice : public LogicalDevice
{
    public:
		virtual bool isCentral();

        InsteonDevice();
        InsteonDevice(uint32_t deviceID, std::string serialNumber, int32_t address);
        virtual ~InsteonDevice();
        virtual DeviceFamilies deviceFamily() { return DeviceFamilies::Insteon; }
        bool packetReceived(std::shared_ptr<Packet> packet);

        virtual void loadVariables();
        virtual void saveVariables();
        virtual void saveVariable(uint32_t index, int64_t intValue);
        virtual void saveVariable(uint32_t index, std::string& stringValue);
        virtual void saveVariable(uint32_t index, std::vector<uint8_t>& binaryValue);

        virtual void sendPacket(std::shared_ptr<InsteonPacket> packet);
    protected:
        std::map<uint32_t, uint32_t> _variableDatabaseIDs;
        bool _disposing = false;

        std::mutex _databaseMutex;
        bool _initialized = false;

        virtual void init();
    private:
};
}
#endif // HMWIREDDEVICE_H
