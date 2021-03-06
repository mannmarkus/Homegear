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

#ifndef PEER_H_
#define PEER_H_

#include "../../RPC/Device.h"
#include "../../Database/Database.h"
class LogicalDevice;

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

class RPCConfigurationParameter
{
public:
	RPCConfigurationParameter() {}
	virtual ~RPCConfigurationParameter() {}

	uint32_t databaseID = 0;
	std::shared_ptr<RPC::Parameter> rpcParameter;
	std::vector<uint8_t> data;
	std::vector<uint8_t> partialData;
};

class ConfigDataBlock
{
public:
	ConfigDataBlock() {}
	virtual ~ConfigDataBlock() {}

	uint32_t databaseID = 0;
	std::vector<uint8_t> data;
};

class Peer
{
public:
	bool deleting = false; //Needed, so the peer gets not saved in central's worker thread while being deleted

	std::unordered_map<uint32_t, ConfigDataBlock> binaryConfig;
	std::unordered_map<uint32_t, std::unordered_map<std::string, RPCConfigurationParameter>> configCentral;
	std::unordered_map<uint32_t, std::unordered_map<std::string, RPCConfigurationParameter>> valuesCentral;
	std::unordered_map<uint32_t, std::unordered_map<int32_t, std::unordered_map<uint32_t, std::unordered_map<std::string, RPCConfigurationParameter>>>> linksCentral;
	std::shared_ptr<RPC::Device> rpcDevice;

	Peer(uint32_t parentID, bool centralFeatures);
	Peer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, bool centralFeatures);
	virtual ~Peer();

	//In table peers:
	int32_t getParentID() { return _parentID; }
	int32_t getAddress() { return _address; }
	uint64_t getID() { return _peerID; }
	void setID(uint64_t ID) { if(_peerID == 0) _peerID = ID; else Output::printError("Cannot reset peer ID"); }
	void setAddress(int32_t value) { _address = value; if(_peerID > 0) save(true, false, false); }
	std::string getSerialNumber() { return _serialNumber; }
	void setSerialNumber(std::string serialNumber) { if(serialNumber.length() > 20) return; _serialNumber = serialNumber; if(_peerID > 0) save(true, false, false); }
	//End

	RPC::Device::RXModes::Enum getRXModes();
	virtual bool isTeam() { return false; }
	void setLastPacketReceived();
	uint32_t getLastPacketReceived() { return _lastPacketReceived; }
	virtual bool pendingQueuesEmpty() { return true; }
	virtual void enqueuePendingQueues() {}

	virtual bool load(LogicalDevice* device) { return false; }
	virtual void save(bool savePeer, bool saveVariables, bool saveCentralConfig);
	void loadConfig();
    void saveConfig();
	virtual void saveParameter(uint32_t parameterID, RPC::ParameterSet::Type::Enum parameterSetType, uint32_t channel, std::string parameterName, std::vector<uint8_t>& value, int32_t remoteAddress = 0, uint32_t remoteChannel = 0);
	virtual void saveParameter(uint32_t parameterID, uint32_t address, std::vector<uint8_t>& value);
	virtual void saveParameter(uint32_t parameterID, std::vector<uint8_t>& value);
	virtual void saveVariables() {}
	virtual void saveVariable(uint32_t index, int32_t intValue);
    virtual void saveVariable(uint32_t index, int64_t intValue);
    virtual void saveVariable(uint32_t index, std::string& stringValue);
    virtual void saveVariable(uint32_t index, std::vector<uint8_t>& binaryValue);
    virtual void deleteFromDatabase();
protected:
    std::map<uint32_t, uint32_t> _variableDatabaseIDs;

	//In table peers:
	uint64_t _peerID = 0;
	uint32_t _parentID = 0;
	int32_t _address = 0;
	std::string _serialNumber;
	//End
	RPC::Device::RXModes::Enum _rxModes = RPC::Device::RXModes::Enum::none;

	bool _disposing = false;
	bool _centralFeatures = false;
	uint32_t _lastPacketReceived = 0;
	std::mutex _databaseMutex;
};

#endif /* PEER_H_ */
