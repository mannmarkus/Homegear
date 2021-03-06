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

#include "HMWiredCentral.h"
#include "../../../GD/GD.h"
#include "../../../../Version.h"

namespace HMWired {

HMWiredCentral::HMWiredCentral() : HMWiredDevice(), Central(0)
{
	init();
}

HMWiredCentral::HMWiredCentral(uint32_t deviceID, std::string serialNumber, int32_t address) : HMWiredDevice(deviceID, serialNumber, address), Central(address)
{
	init();
}

HMWiredCentral::~HMWiredCentral()
{
	dispose();

	if(_updateFirmwareThread.joinable()) _updateFirmwareThread.join();
}

void HMWiredCentral::init()
{
	try
	{
		HMWiredDevice::init();

		_deviceType = (uint32_t)DeviceType::HMWIREDCENTRAL;
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

bool HMWiredCentral::packetReceived(std::shared_ptr<Packet> packet)
{
	try
	{
		if(_disposing) return false;
		HMWiredDevice::packetReceived(packet);
		std::shared_ptr<HMWiredPacket> hmWiredPacket(std::dynamic_pointer_cast<HMWiredPacket>(packet));
		if(!hmWiredPacket) return false;
		std::shared_ptr<HMWiredPeer> peer(getPeer(hmWiredPacket->senderAddress()));
		if(peer) peer->packetReceived(hmWiredPacket);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return false;
}

void HMWiredCentral::deletePeer(uint64_t id)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(id));
		if(!peer) return;
		peer->deleting = true;
		std::shared_ptr<RPC::RPCVariable> deviceAddresses(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		deviceAddresses->arrayValue->push_back(std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(peer->getSerialNumber())));
		for(std::map<uint32_t, std::shared_ptr<RPC::DeviceChannel>>::iterator i = peer->rpcDevice->channels.begin(); i != peer->rpcDevice->channels.end(); ++i)
		{
			deviceAddresses->arrayValue->push_back(std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(peer->getSerialNumber() + ":" + std::to_string(i->first))));
		}
		std::shared_ptr<RPC::RPCVariable> deviceInfo(new RPC::RPCVariable(RPC::RPCVariableType::rpcStruct));
		deviceInfo->structValue->insert(RPC::RPCStructElement("ID", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable((int32_t)peer->getID()))));
		std::shared_ptr<RPC::RPCVariable> channels(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		deviceInfo->structValue->insert(RPC::RPCStructElement("CHANNELS", channels));
		for(std::map<uint32_t, std::shared_ptr<RPC::DeviceChannel>>::iterator i = peer->rpcDevice->channels.begin(); i != peer->rpcDevice->channels.end(); ++i)
		{
			channels->arrayValue->push_back(std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(i->first)));
		}
		GD::rpcClient.broadcastDeleteDevices(deviceAddresses, deviceInfo);
		Metadata::deleteMetadata(peer->getSerialNumber());
		Metadata::deleteMetadata(std::to_string(id));
		if(peer->rpcDevice)
		{
			for(std::map<uint32_t, std::shared_ptr<RPC::DeviceChannel>>::iterator i = peer->rpcDevice->channels.begin(); i != peer->rpcDevice->channels.end(); ++i)
			{
				Metadata::deleteMetadata(peer->getSerialNumber() + ':' + std::to_string(i->first));
				Metadata::deleteMetadata(std::to_string(id) + ':' + std::to_string(i->first));
			}
		}
		peer->deleteFromDatabase();
		_peersMutex.lock();
		if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
		if(_peers.find(peer->getAddress()) != _peers.end()) _peers.erase(peer->getAddress());
		if(_peersByID.find(id) != _peersByID.end()) _peersByID.erase(id);
		_peersMutex.unlock();
		Output::printMessage("Removed HomeMatic Wired peer " + std::to_string(peer->getID()));
	}
	catch(const std::exception& ex)
    {
		_peersMutex.unlock();
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_peersMutex.unlock();
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_peersMutex.unlock();
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::string HMWiredCentral::handleCLICommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;
		if(_currentPeer)
		{
			if(command == "unselect")
			{
				_currentPeer.reset();
				return "Peer unselected.\n";
			}
			if(!_currentPeer) return "No peer selected.\n";
			return _currentPeer->handleCLICommand(command);
		}
		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the indivual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "peers add\t\tManually adds a peer (without pairing it! Only for testing)" << std::endl;
			stringStream << "peers list\t\tList all peers" << std::endl;
			stringStream << "peers reset\t\tUnpair a peer and reset it to factory defaults" << std::endl;
			stringStream << "peers select\t\tSelect a peer" << std::endl;
			stringStream << "peers unpair\t\tUnpair a peer" << std::endl;
			stringStream << "peers update\t\tUpdates a peer to the newest firmware version" << std::endl;
			stringStream << "search\t\t\tSearches for new devices on the bus" << std::endl;
			stringStream << "unselect\t\tUnselect this device" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 6, "search") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command searches for new devices on the bus." << std::endl;
						stringStream << "Usage: search" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			std::shared_ptr<RPC::RPCVariable> result = searchDevices();
			if(result->errorStruct) stringStream << "Error: " << result->structValue->at("faultString")->stringValue << std::endl;
			else stringStream << "Search completed successfully." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers unpair") == 0)
		{
			uint64_t peerID;

			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help") break;
					peerID = HelperFunctions::getNumber(element, false);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 2)
			{
				stringStream << "Description: This command unpairs a peer." << std::endl;
				stringStream << "Usage: peers unpair PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to unpair. Example: 513" << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				if(_currentPeer && _currentPeer->getID() == peerID) _currentPeer.reset();
				stringStream << "Unpairing peer " << std::to_string(peerID) << std::endl;
				deletePeer(peerID);
			}
			return stringStream.str();
		}
		else if(command.compare(0, 11, "peers reset") == 0)
		{
			uint64_t peerID;

			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help") break;
					peerID = HelperFunctions::getNumber(element, false);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 2)
			{
				stringStream << "Description: This command resets and unpairs a peer." << std::endl;
				stringStream << "Usage: peers reset PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to reset. Example: 513" << std::endl;
				return stringStream.str();
			}

			std::shared_ptr<HMWiredPeer> peer = getPeer(peerID);
			if(!peer) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				if(_currentPeer && _currentPeer->getID() == peerID) _currentPeer.reset();
				stringStream << "Resetting peer " << std::to_string(peerID) << std::endl;
				peer->reset();
				deletePeer(peerID);
			}
			return stringStream.str();
		}
		else if(command.compare(0, 10, "peers list") == 0)
		{
			try
			{
				std::string filterType;
				std::string filterValue;

				std::stringstream stream(command);
				std::string element;
				int32_t index = 0;
				while(std::getline(stream, element, ' '))
				{
					if(index < 2)
					{
						index++;
						continue;
					}
					else if(index == 2)
					{
						if(element == "help")
						{
							index = -1;
							break;
						}
						filterType = HelperFunctions::toLower(element);
					}
					else if(index == 3) filterValue = element;
					index++;
				}
				if(index == -1)
				{
					stringStream << "Description: This command unpairs a peer." << std::endl;
					stringStream << "Usage: peers list [FILTERTYPE] [FILTERVALUE]" << std::endl << std::endl;
					stringStream << "Parameters:" << std::endl;
					stringStream << "  FILTERTYPE:\tSee filter types below." << std::endl;
					stringStream << "  FILTERVALUE:\tDepends on the filter type. If a number is required, it has to be in hexadecimal format." << std::endl << std::endl;
					stringStream << "Filter types:" << std::endl;
					stringStream << "  PEERID: Filter by id." << std::endl;
					stringStream << "      FILTERVALUE: The id of the peer to filter (e. g. 513)." << std::endl;
					stringStream << "  ADDRESS: Filter by address." << std::endl;
					stringStream << "      FILTERVALUE: The 4 byte address of the peer to filter (e. g. 001DA44D)." << std::endl;
					stringStream << "  SERIAL: Filter by serial number." << std::endl;
					stringStream << "      FILTERVALUE: The serial number of the peer to filter (e. g. JEQ0554309)." << std::endl;
					stringStream << "  TYPE: Filter by device type." << std::endl;
					stringStream << "      FILTERVALUE: The 2 byte device type in hexadecimal format." << std::endl;
					stringStream << "  UNREACH: List all unreachable peers." << std::endl;
					stringStream << "      FILTERVALUE: empty" << std::endl;
					return stringStream.str();
				}

				if(_peers.empty())
				{
					stringStream << "No peers are paired to this central." << std::endl;
					return stringStream.str();
				}
				bool firmwareUpdates = false;
				std::string bar(" │ ");
				const int32_t idWidth = 8;
				const int32_t addressWidth = 8;
				const int32_t serialWidth = 13;
				const int32_t typeWidth1 = 4;
				const int32_t typeWidth2 = 25;
				const int32_t firmwareWidth = 8;
				const int32_t unreachWidth = 7;
				std::string typeStringHeader("Type String");
				typeStringHeader.resize(typeWidth2, ' ');
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << "ID" << bar
					<< std::setw(addressWidth) << "Address" << bar
					<< std::setw(serialWidth) << "Serial Number" << bar
					<< std::setw(typeWidth1) << "Type" << bar
					<< typeStringHeader << bar
					<< std::setw(firmwareWidth) << "Firmware" << bar
					<< std::setw(unreachWidth) << "Unreach"
					<< std::endl;
				stringStream << "─────────┼──────────┼───────────────┼──────┼───────────────────────────┼──────────┼────────" << std::endl;
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << " " << bar
					<< std::setw(addressWidth) << " " << bar
					<< std::setw(serialWidth) << " " << bar
					<< std::setw(typeWidth1) << " " << bar
					<< std::setw(typeWidth2) << " " << bar
					<< std::setw(firmwareWidth) << " " << bar
					<< std::setw(unreachWidth) << " "
					<< std::endl;
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<HMWiredPeer>>::iterator i = _peersByID.begin(); i != _peersByID.end(); ++i)
				{
					if(filterType == "id")
					{
						uint64_t id = HelperFunctions::getNumber(filterValue, true);
						if(i->second->getID() != id) continue;
					}
					else if(filterType == "address")
					{
						int32_t address = HelperFunctions::getNumber(filterValue, true);
						if(i->second->getAddress() != address) continue;
					}
					else if(filterType == "serial")
					{
						if(i->second->getSerialNumber() != filterValue) continue;
					}
					else if(filterType == "type")
					{
						int32_t deviceType = HelperFunctions::getNumber(filterValue, true);
						if((int32_t)i->second->getDeviceType().type() != deviceType) continue;
					}
					else if(filterType == "unreach")
					{
						if(i->second->serviceMessages)
						{
							if(!i->second->serviceMessages->getUnreach()) continue;
						}
					}

					stringStream
						<< std::setw(idWidth) << std::setfill(' ') << std::to_string(i->second->getID()) << bar
						<< std::setw(addressWidth) << HelperFunctions::getHexString(i->second->getAddress(), 8) << bar
						<< std::setw(serialWidth) << i->second->getSerialNumber() << bar
						<< std::setw(typeWidth1) << HelperFunctions::getHexString(i->second->getDeviceType().type(), 4) << bar;
					if(i->second->rpcDevice)
					{
						std::shared_ptr<RPC::DeviceType> type = i->second->rpcDevice->getType(i->second->getDeviceType(), i->second->getFirmwareVersion());
						std::string typeID;
						if(type) typeID = type->id;
						typeID.resize(typeWidth2, ' ');
						stringStream << typeID  << bar;
					}
					else stringStream << std::setw(typeWidth2) << " " << bar;
					if(i->second->getFirmwareVersion() == 0) stringStream << std::setfill(' ') << std::setw(firmwareWidth) << "?" << bar;
					else if(i->second->firmwareUpdateAvailable())
					{
						stringStream << std::setfill(' ') << std::setw(firmwareWidth) << ("*" + HelperFunctions::getHexString(i->second->getFirmwareVersion() >> 8) + "." + HelperFunctions::getHexString(i->second->getFirmwareVersion() & 0xFF, 2)) << bar;
						firmwareUpdates = true;
					}
					else stringStream << std::setfill(' ') << std::setw(firmwareWidth) << (HelperFunctions::getHexString(i->second->getFirmwareVersion() >> 8) + "." + HelperFunctions::getHexString(i->second->getFirmwareVersion() & 0xFF, 2)) << bar;
					if(i->second->serviceMessages)
					{
						std::string unreachable(i->second->serviceMessages->getUnreach() ? "Yes" : "No");
						stringStream << std::setw(unreachWidth) << unreachable;
					}
					stringStream << std::endl << std::dec;
				}
				_peersMutex.unlock();
				stringStream << "─────────┴──────────┴───────────────┴──────┴───────────────────────────┴──────────┴────────" << std::endl;
				if(firmwareUpdates) stringStream << std::endl << "*: Firmware update available." << std::endl;

				return stringStream.str();
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(Exception& ex)
			{
				_peersMutex.unlock();
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_peersMutex.unlock();
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
		else if(command.compare(0, 12, "peers update") == 0)
		{
			uint64_t peerID;
			bool all = false;

			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help") break;
					else if(element == "all") all = true;
					else
					{
						peerID = HelperFunctions::getNumber(element, false);
						if(peerID == 0) return "Invalid id.\n";
					}
				}
				index++;
			}
			if(index == 2)
			{
				stringStream << "Description: This command updates one or all peers to the newest firmware version available in \"" << GD::settings.firmwarePath() << "\"." << std::endl;
				stringStream << "Usage: peers update PEERID" << std::endl;
				stringStream << "       peers update all" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to update. Example: 513" << std::endl;
				return stringStream.str();
			}

			std::shared_ptr<RPC::RPCVariable> result;
			std::vector<uint64_t> ids;
			if(all)
			{
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<HMWiredPeer>>::iterator i = _peersByID.begin(); i != _peersByID.end(); ++i)
				{
					if(i->second->firmwareUpdateAvailable()) ids.push_back(i->first);
				}
				_peersMutex.unlock();
				if(ids.empty())
				{
					stringStream << "All peers are up to date." << std::endl;
					return stringStream.str();
				}
				result = updateFirmware(ids, false);
			}
			else if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				std::shared_ptr<HMWiredPeer> peer = getPeer(peerID);
				if(!peer->firmwareUpdateAvailable())
				{
					stringStream << "Peer is up to date." << std::endl;
					return stringStream.str();
				}
				ids.push_back(peerID);
				result = updateFirmware(ids, false);
			}
			if(!result) stringStream << "Unknown error." << std::endl;
			else if(result->errorStruct) stringStream << result->structValue->at("faultString")->stringValue << std::endl;
			else stringStream << "Started firmware update(s)... This might take a long time. Use the RPC function \"getUpdateProgress\" or see the log for details." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers select") == 0)
		{
			uint64_t id;

			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help") break;
					id = HelperFunctions::getNumber(element, false);
					if(id == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 2)
			{
				stringStream << "Description: This command selects a peer." << std::endl;
				stringStream << "Usage: peers select PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to select. Example: 513" << std::endl;
				return stringStream.str();
			}

			_currentPeer = getPeer(id);
			if(!_currentPeer) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				stringStream << "Peer with id " << std::hex << std::to_string(id) << " and device type 0x" << (int32_t)_currentPeer->getDeviceType().type() << " selected." << std::dec << std::endl;
				stringStream << "For information about the peer's commands type: \"help\"" << std::endl;
			}
			return stringStream.str();
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return "Error executing command. See log file for more details.\n";
}

std::shared_ptr<HMWiredPeer> HMWiredCentral::createPeer(int32_t address, int32_t firmwareVersion, LogicalDeviceType deviceType, std::string serialNumber, bool save)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(new HMWiredPeer(_deviceID, true));
		peer->setAddress(address);
		peer->setFirmwareVersion(firmwareVersion);
		peer->setDeviceType(deviceType);
		peer->setSerialNumber(serialNumber);
		peer->rpcDevice = GD::rpcDevices.find(deviceType, firmwareVersion, -1);
		if(!peer->rpcDevice) return std::shared_ptr<HMWiredPeer>();
		if(save) peer->save(true, true, false); //Save and create peerID
		return peer;
	}
    catch(const std::exception& ex)
    {
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<HMWiredPeer>();
}

void HMWiredCentral::updateFirmwares(std::vector<uint64_t> ids)
{
	try
	{
		if(_updateMode || GD::devices.updateInfo.currentDevice > 0) return;
		GD::devices.updateInfo.updateMutex.lock();
		GD::devices.updateInfo.devicesToUpdate = ids.size();
		GD::devices.updateInfo.currentUpdate = 0;
		for(std::vector<uint64_t>::iterator i = ids.begin(); i != ids.end(); ++i)
		{
			GD::devices.updateInfo.currentDeviceProgress = 0;
			GD::devices.updateInfo.currentUpdate++;
			GD::devices.updateInfo.currentDevice = *i;
			updateFirmware(*i);
		}
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	GD::devices.updateInfo.reset();
	GD::devices.updateInfo.updateMutex.unlock();
}

void HMWiredCentral::updateFirmware(uint64_t id)
{
	try
	{
		if(_updateMode) return;
		std::shared_ptr<HMWiredPeer> peer = getPeer(id);
		if(!peer) return;
		_updateMode = true;
		_updateMutex.lock();
		std::string filenamePrefix = HelperFunctions::getHexString((int32_t)DeviceFamilies::HomeMaticWired, 4) + "." + HelperFunctions::getHexString(peer->getDeviceType().type(), 8);
		std::string versionFile(GD::settings.firmwarePath() + filenamePrefix + ".version");
		if(!HelperFunctions::fileExists(versionFile))
		{
			Output::printInfo("Info: Not updating peer with id " + std::to_string(id) + ". No version info file found.");
			GD::devices.updateInfo.results[id].first = 2;
			GD::devices.updateInfo.results[id].second = "No version file found.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		std::string firmwareFile(GD::settings.firmwarePath() + filenamePrefix + ".fw");
		if(!HelperFunctions::fileExists(firmwareFile))
		{
			Output::printInfo("Info: Not updating peer with id " + std::to_string(id) + ". No firmware file found.");
			GD::devices.updateInfo.results[id].first = 3;
			GD::devices.updateInfo.results[id].second = "No firmware file found.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		int32_t firmwareVersion = peer->getNewFirmwareVersion();
		if(peer->getFirmwareVersion() >= firmwareVersion)
		{
			GD::devices.updateInfo.results[id].first = 0;
			GD::devices.updateInfo.results[id].second = "Already up to date.";
			Output::printInfo("Info: Not updating peer with id " + std::to_string(id) + ". Peer firmware is already up to date.");
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		std::string oldVersionString = HelperFunctions::getHexString(peer->getFirmwareVersion() >> 8) + "." + HelperFunctions::getHexString(peer->getFirmwareVersion() & 0xFF, 2);
		std::string versionString = HelperFunctions::getHexString(firmwareVersion >> 8) + "." + HelperFunctions::getHexString(firmwareVersion & 0xFF, 2);

		std::string firmwareHex;
		try
		{
			firmwareHex = HelperFunctions::getFileContent(firmwareFile);
		}
		catch(const std::exception& ex)
		{
			Output::printError("Error: Could not open firmware file: " + firmwareFile + ": " + ex.what());
			GD::devices.updateInfo.results[id].first = 4;
			GD::devices.updateInfo.results[id].second = "Could not open firmware file.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		catch(...)
		{
			Output::printError("Error: Could not open firmware file: " + firmwareFile + ".");
			GD::devices.updateInfo.results[id].first = 4;
			GD::devices.updateInfo.results[id].second = "Could not open firmware file.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}

		std::stringstream stream(firmwareHex);
		std::string line;
		int32_t currentAddress = 0;
		std::vector<uint8_t> firmware;
		while(std::getline(stream, line))
		{
			if(line.at(0) != ':' || line.size() < 11)
			{
				GD::devices.updateInfo.results[id].first = 5;
				GD::devices.updateInfo.results[id].second = "Firmware file has wrong format.";
				Output::printError("Error: Could not read firmware file: " + firmwareFile + ": Wrong format (no colon at position 0 or line too short).");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
			std::string hex = line.substr(1, 2);
			int32_t bytes = HelperFunctions::getNumber(hex, true);
			hex = line.substr(7, 2);
			int32_t recordType = HelperFunctions::getNumber(hex, true);
			if(recordType == 1) break; //End of file
			if(recordType != 0)
			{
				GD::devices.updateInfo.results[id].first = 5;
				GD::devices.updateInfo.results[id].second = "Firmware file has wrong format.";
				Output::printError("Error: Could not read firmware file: " + firmwareFile + ": Wrong format (wrong record type).");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
			hex = line.substr(3, 4);
			int32_t address = HelperFunctions::getNumber(hex, true);
			if(address != currentAddress || (11 + bytes * 2) > line.size())
			{
				GD::devices.updateInfo.results[id].first = 5;
				GD::devices.updateInfo.results[id].second = "Firmware file has wrong format.";
				Output::printError("Error: Could not read firmware file: " + firmwareFile + ": Wrong format (address does not match).");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
			currentAddress += bytes;
			std::vector<uint8_t> data = HelperFunctions::getUBinary(line.substr(9, bytes * 2));
			hex = line.substr(9 + bytes * 2, 2);
			int32_t checkSum = HelperFunctions::getNumber(hex, true);
			int32_t calculatedCheckSum = bytes + (address >> 8) + (address & 0xFF) + recordType;
			for(std::vector<uint8_t>::iterator i = data.begin(); i != data.end(); ++i)
			{
				calculatedCheckSum += *i;
			}
			calculatedCheckSum = (((calculatedCheckSum & 0xFF) ^ 0xFF) + 1) & 0xFF;
			if(calculatedCheckSum != checkSum)
			{
				GD::devices.updateInfo.results[id].first = 5;
				GD::devices.updateInfo.results[id].second = "Firmware file has wrong format.";
				Output::printError("Error: Could not read firmware file: " + firmwareFile + ": Wrong format (check sum failed).");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
			firmware.insert(firmware.end(), data.begin(), data.end());
		}

		lockBus();

		std::shared_ptr<HMWiredPacket> response = getResponse(0x75, peer->getAddress(), true);
		if(!response || response->type() != HMWiredPacketType::ackMessage)
		{
			unlockBus();
			GD::devices.updateInfo.results[id].first = 6;
			GD::devices.updateInfo.results[id].second = "Device did not respond to enter-bootloader packet.";
			Output::printWarning("Warning: Device did not enter bootloader.");
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}

		//Wait for the device to enter bootloader
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		std::vector<uint8_t> payload;
		payload.push_back(0x75);
		std::shared_ptr<HMWiredPacket> packet(new HMWiredPacket(HMWiredPacketType::iMessage, 0, peer->getAddress(), false, getMessageCounter(peer->getAddress()), 0, 0, payload));
		response = getResponse(packet, true);
		if(!response || response->type() != HMWiredPacketType::system)
		{
			unlockBus();
			GD::devices.updateInfo.results[id].first = 6;
			GD::devices.updateInfo.results[id].second = "Device did not respond to enter-bootloader packet.";
			Output::printWarning("Warning: Device did not enter bootloader.");
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}

		payload.clear();
		payload.push_back(0x70);
		packet.reset(new HMWiredPacket(HMWiredPacketType::iMessage, 0, peer->getAddress(), false, getMessageCounter(peer->getAddress()), 0, 0, payload));
		response = getResponse(packet, true);
		int32_t packetSize = 0;
		if(response && response->payload()->size() == 2) packetSize = (response->payload()->at(0) << 8) + response->payload()->at(1);
		if(!response || response->type() != HMWiredPacketType::system || response->payload()->size() != 2 || packetSize > 128 || packetSize == 0)
		{
			unlockBus();
			GD::devices.updateInfo.results[id].first = 8;
			GD::devices.updateInfo.results[id].second = "Too many communication errors (block size request failed).";
			Output::printWarning("Error: Block size request failed.");
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}

		std::vector<uint8_t> data;
		for(int32_t i = 0; i < firmware.size(); i += packetSize)
		{
			GD::devices.updateInfo.currentDeviceProgress = (i * 100) / firmware.size();
			int32_t currentPacketSize = (i + packetSize < firmware.size()) ? packetSize : firmware.size() - i;
			data.clear();
			data.push_back(0x77); //Type
			data.push_back(i >> 8); //Address
			data.push_back(i & 0xFF); //Address
			data.push_back(currentPacketSize); //Length
			data.insert(data.end(), firmware.begin() + i, firmware.begin() + i + currentPacketSize);

			std::shared_ptr<HMWiredPacket> packet(new HMWiredPacket(HMWiredPacketType::iMessage, 0, peer->getAddress(), false, getMessageCounter(peer->getAddress()), 0, 0, data));
			response = getResponse(packet, true);
			if(!response || response->type() != HMWiredPacketType::system || response->payload()->size() != 2)
			{
				unlockBus();
				GD::devices.updateInfo.results[id].first = 8;
				GD::devices.updateInfo.results[id].second = "Too many communication errors.";
				Output::printWarning("Error: Block size request failed.");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
			int32_t receivedBytes = (response->payload()->at(0) << 8) + response->payload()->at(1);
			if(receivedBytes != currentPacketSize)
			{
				unlockBus();
				GD::devices.updateInfo.results[id].first = 8;
				GD::devices.updateInfo.results[id].second = "Too many communication errors (device received wrong number of bytes).";
				Output::printWarning("Error: Block size request failed.");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
		}

		payload.clear();
		payload.push_back(0x67);
		packet.reset(new HMWiredPacket(HMWiredPacketType::iMessage, 0, peer->getAddress(), false, getMessageCounter(peer->getAddress()), 0, 0, payload));
		for(int32_t i = 0; i < 3; i++)
		{
			sendPacket(packet, false);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		unlockBus();

		peer->setFirmwareVersion(firmwareVersion);
		GD::devices.updateInfo.results[id].first = 0;
		GD::devices.updateInfo.results[id].second = "Update successful.";
		Output::printInfo("Info: Peer " + std::to_string(id) + " was successfully updated to firmware version " + versionString + ".");
		_updateMutex.unlock();
		_updateMode = false;
		return;
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    unlockBus();
    GD::devices.updateInfo.results[id].first = 1;
	GD::devices.updateInfo.results[id].second = "Unknown error.";
    _updateMutex.unlock();
    _updateMode = false;
}

bool HMWiredCentral::knowsDevice(std::string serialNumber)
{
	if(serialNumber == _serialNumber) return true;
	_peersMutex.lock();
	try
	{
		if(_peersBySerial.find(serialNumber) != _peersBySerial.end())
		{
			_peersMutex.unlock();
			return true;
		}
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	_peersMutex.unlock();
	return false;
}

bool HMWiredCentral::knowsDevice(uint64_t id)
{
	_peersMutex.lock();
	try
	{
		if(_peersByID.find(id) != _peersByID.end())
		{
			_peersMutex.unlock();
			return true;
		}
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	_peersMutex.unlock();
	return false;
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::addLink(std::string senderSerialNumber, int32_t senderChannelIndex, std::string receiverSerialNumber, int32_t receiverChannelIndex, std::string name, std::string description)
{
	try
	{
		if(senderSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<HMWiredPeer> sender = getPeer(senderSerialNumber);
		std::shared_ptr<HMWiredPeer> receiver = getPeer(receiverSerialNumber);
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		return addLink(sender->getID(), senderChannelIndex, receiver->getID(), receiverChannelIndex, name, description);
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::addLink(uint64_t senderID, int32_t senderChannelIndex, uint64_t receiverID, int32_t receiverChannelIndex, std::string name, std::string description)
{
	try
	{
		if(senderID == 0) return RPC::RPCVariable::createError(-2, "Given sender id is not set.");
		if(receiverID == 0) return RPC::RPCVariable::createError(-2, "Given receiver id is not set.");
		std::shared_ptr<HMWiredPeer> sender = getPeer(senderID);
		std::shared_ptr<HMWiredPeer> receiver = getPeer(receiverID);
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		if(senderChannelIndex < 0) senderChannelIndex = 0;
		if(receiverChannelIndex < 0) receiverChannelIndex = 0;
		if(sender->rpcDevice->channels.find(senderChannelIndex) == sender->rpcDevice->channels.end()) return RPC::RPCVariable::createError(-2, "Sender channel not found.");
		if(receiver->rpcDevice->channels.find(receiverChannelIndex) == receiver->rpcDevice->channels.end()) return RPC::RPCVariable::createError(-2, "Receiver channel not found.");
		std::shared_ptr<RPC::DeviceChannel> senderChannel = sender->rpcDevice->channels.at(senderChannelIndex);
		std::shared_ptr<RPC::DeviceChannel> receiverChannel = receiver->rpcDevice->channels.at(receiverChannelIndex);
		if(senderChannel->linkRoles->sourceNames.size() == 0 || receiverChannel->linkRoles->targetNames.size() == 0) RPC::RPCVariable::createError(-6, "Link not supported.");
		if(sender->getPeer(senderChannelIndex, receiver->getID(), receiverChannelIndex) || receiver->getPeer(receiverChannelIndex, sender->getID(), senderChannelIndex)) return RPC::RPCVariable::createError(-6, "Link already exists.");
		bool validLink = false;
		for(std::vector<std::string>::iterator i = senderChannel->linkRoles->sourceNames.begin(); i != senderChannel->linkRoles->sourceNames.end(); ++i)
		{
			for(std::vector<std::string>::iterator j = receiverChannel->linkRoles->targetNames.begin(); j != receiverChannel->linkRoles->targetNames.end(); ++j)
			{
				if(*i == *j)
				{
					validLink = true;
					break;
				}
			}
			if(validLink) break;
		}

		std::shared_ptr<BasicPeer> senderPeer(new BasicPeer());
		senderPeer->isSender = true;
		senderPeer->id = sender->getID();
		senderPeer->address = sender->getAddress();
		senderPeer->channel = senderChannelIndex;
		senderPeer->physicalIndexOffset = senderChannel->physicalIndexOffset;
		senderPeer->serialNumber = sender->getSerialNumber();
		senderPeer->linkDescription = description;
		senderPeer->linkName = name;
		senderPeer->configEEPROMAddress = receiver->getFreeEEPROMAddress(receiverChannelIndex, false);
		if(senderPeer->configEEPROMAddress == -1) return RPC::RPCVariable::createError(-32500, "Can't get free eeprom address to store config.");

		std::shared_ptr<BasicPeer> receiverPeer(new BasicPeer());
		receiverPeer->id = receiver->getID();
		receiverPeer->address = receiver->getAddress();
		receiverPeer->channel = receiverChannelIndex;
		receiverPeer->physicalIndexOffset = receiverChannel->physicalIndexOffset;
		receiverPeer->serialNumber = receiver->getSerialNumber();
		receiverPeer->linkDescription = description;
		receiverPeer->linkName = name;
		receiverPeer->configEEPROMAddress = sender->getFreeEEPROMAddress(senderChannelIndex, true);
		if(receiverPeer->configEEPROMAddress == -1) return RPC::RPCVariable::createError(-32500, "Can't get free eeprom address to store config.");

		sender->addPeer(senderChannelIndex, receiverPeer);
		sender->initializeLinkConfig(senderChannelIndex, receiverPeer);
		GD::rpcClient.broadcastUpdateDevice(sender->getID(), senderChannelIndex, sender->getSerialNumber() + ":" + std::to_string(senderChannelIndex), RPC::Client::Hint::Enum::updateHintLinks);

		receiver->addPeer(receiverChannelIndex, senderPeer);
		receiver->initializeLinkConfig(receiverChannelIndex, senderPeer);
		GD::rpcClient.broadcastUpdateDevice(receiver->getID(), receiverChannelIndex, receiver->getSerialNumber() + ":" + std::to_string(receiverChannelIndex), RPC::Client::Hint::Enum::updateHintLinks);

		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::deleteDevice(std::string serialNumber, int32_t flags)
{
	try
	{
		if(serialNumber.empty()) return RPC::RPCVariable::createError(-2, "Unknown device.");
		std::shared_ptr<HMWiredPeer> peer = getPeer(serialNumber);
		if(!peer) return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));

		return deleteDevice(peer->getID(), flags);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::deleteDevice(uint64_t peerID, int32_t flags)
{
	try
	{
		if(peerID == 0) return RPC::RPCVariable::createError(-2, "Unknown device.");
		std::shared_ptr<HMWiredPeer> peer = getPeer(peerID);
		if(!peer) return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
		uint64_t id = peer->getID();

		bool defer = flags & 0x04;
		bool force = flags & 0x02;
		//Reset
		if(flags & 0x01) peer->reset();
		deletePeer(id);

		if(knowsDevice(id)) return RPC::RPCVariable::createError(-1, "Error deleting peer. See log for more details.");

		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getDeviceDescription(std::string serialNumber, int32_t channel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");

		return peer->getDeviceDescription(channel);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getDeviceDescription(uint64_t id, int32_t channel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(id));
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");

		return peer->getDeviceDescription(channel);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

/*std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getDeviceDescriptionCentral()
{
	try
	{
		std::shared_ptr<RPC::RPCVariable> description(new RPC::RPCVariable(RPC::RPCVariableType::rpcStruct));

		description->structValue->insert(RPC::RPCStructElement("ID", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(_deviceID))));
		description->structValue->insert(RPC::RPCStructElement("ADDRESS", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(_serialNumber))));

		std::shared_ptr<RPC::RPCVariable> variable = std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		description->structValue->insert(RPC::RPCStructElement("CHILDREN", variable));

		description->structValue->insert(RPC::RPCStructElement("FIRMWARE", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::string(VERSION)))));

		int32_t uiFlags = (int32_t)RPC::Device::UIFlags::dontdelete | (int32_t)RPC::Device::UIFlags::visible;
		description->structValue->insert(RPC::RPCStructElement("FLAGS", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(uiFlags))));

		description->structValue->insert(RPC::RPCStructElement("INTERFACE", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(_serialNumber))));

		variable = std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		description->structValue->insert(RPC::RPCStructElement("PARAMSETS", variable));
		variable->arrayValue->push_back(std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::string("MASTER")))); //Always MASTER

		description->structValue->insert(RPC::RPCStructElement("PARENT", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::string("")))));

		description->structValue->insert(RPC::RPCStructElement("PHYSICAL_ADDRESS", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::to_string(_address)))));
		description->structValue->insert(RPC::RPCStructElement("RF_ADDRESS", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::to_string(_address)))));

		description->structValue->insert(RPC::RPCStructElement("ROAMING", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(0))));

		description->structValue->insert(RPC::RPCStructElement("TYPE", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::string("Homegear HomeMatic Wired Central")))));

		description->structValue->insert(RPC::RPCStructElement("VERSION", std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable((int32_t)10))));

		return description;
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}*/

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getLinkInfo(std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel)
{
	try
	{
		if(senderSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<HMWiredPeer> sender(getPeer(senderSerialNumber));
		std::shared_ptr<HMWiredPeer> receiver(getPeer(receiverSerialNumber));
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		return sender->getLinkInfo(senderChannel, receiver->getID(), receiverChannel);
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getLinkInfo(uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel)
{
	try
	{
		if(senderID == 0) return RPC::RPCVariable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return RPC::RPCVariable::createError(-2, "Receiver id is not set.");
		std::shared_ptr<HMWiredPeer> sender(getPeer(senderID));
		std::shared_ptr<HMWiredPeer> receiver(getPeer(receiverID));
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		return sender->getLinkInfo(senderChannel, receiver->getID(), receiverChannel);
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getLinkPeers(std::string serialNumber, int32_t channel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
		return peer->getLinkPeers(channel, false);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getLinkPeers(uint64_t peerID, int32_t channel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(peerID));
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
		return peer->getLinkPeers(channel, true);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getLinks(std::string serialNumber, int32_t channel, int32_t flags)
{
	try
	{
		if(serialNumber.empty()) return getLinks(0, -1, flags);
		std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
		return getLinks(peer->getID(), channel, flags);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getLinks(uint64_t peerID, int32_t channel, int32_t flags)
{
	try
	{
		std::shared_ptr<RPC::RPCVariable> array(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		std::shared_ptr<RPC::RPCVariable> element(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		if(peerID == 0)
		{
			try
			{
				std::vector<std::shared_ptr<HMWiredPeer>> peers;
				//Copy all peers first, because getLinks takes very long and we don't want to lock _peersMutex too long
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<HMWiredPeer>>::iterator i = _peersByID.begin(); i != _peersByID.end(); ++i)
				{
					peers.push_back(i->second);
				}
				_peersMutex.unlock();

				for(std::vector<std::shared_ptr<HMWiredPeer>>::iterator i = peers.begin(); i != peers.end(); ++i)
				{
					//listDevices really needs a lot of ressources, so wait a little bit after each device
					std::this_thread::sleep_for(std::chrono::milliseconds(3));
					element = (*i)->getLink(channel, flags, true);
					array->arrayValue->insert(array->arrayValue->begin(), element->arrayValue->begin(), element->arrayValue->end());
				}
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(Exception& ex)
			{
				_peersMutex.unlock();
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_peersMutex.unlock();
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
		else
		{
			std::shared_ptr<HMWiredPeer> peer(getPeer(peerID));
			if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
			element = peer->getLink(channel, flags, false);
			array->arrayValue->insert(array->arrayValue->begin(), element->arrayValue->begin(), element->arrayValue->end());
		}
		return array;
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getParamset(std::string serialNumber, int32_t channel, RPC::ParameterSet::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel)
{
	try
	{
		if(serialNumber == _serialNumber && (channel == 0 || channel == -1) && type == RPC::ParameterSet::Type::Enum::master)
		{
			std::shared_ptr<RPC::RPCVariable> paramset(new RPC::RPCVariable(RPC::RPCVariableType::rpcStruct));
			return paramset;
		}
		else
		{
			std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
			uint64_t remoteID = 0;
			if(!remoteSerialNumber.empty())
			{
				std::shared_ptr<HMWiredPeer> remotePeer(getPeer(remoteSerialNumber));
				if(!remotePeer) return RPC::RPCVariable::createError(-3, "Remote peer is unknown.");
				remoteID = remotePeer->getID();
			}
			if(peer) return peer->getParamset(channel, type, remoteID, remoteChannel);
			return RPC::RPCVariable::createError(-2, "Unknown device.");
		}
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getParamset(uint64_t peerID, int32_t channel, RPC::ParameterSet::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(peerID));
		if(peer) return peer->getParamset(channel, type, remoteID, remoteChannel);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getParamsetDescription(std::string serialNumber, int32_t channel, RPC::ParameterSet::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel)
{
	try
	{
		if(serialNumber == _serialNumber && (channel == 0 || channel == -1) && type == RPC::ParameterSet::Type::Enum::master)
		{
			std::shared_ptr<RPC::RPCVariable> descriptions(new RPC::RPCVariable(RPC::RPCVariableType::rpcStruct));
			return descriptions;
		}
		else
		{
			std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
			uint64_t remoteID = 0;
			if(!remoteSerialNumber.empty())
			{
				std::shared_ptr<HMWiredPeer> remotePeer(getPeer(remoteSerialNumber));
				if(remotePeer) remoteID = remotePeer->getID();
			}
			if(peer) return peer->getParamsetDescription(channel, type, remoteID, remoteChannel);
			return RPC::RPCVariable::createError(-2, "Unknown device.");
		}
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getParamsetDescription(uint64_t peerID, int32_t channel, RPC::ParameterSet::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(peerID));
		if(peer) return peer->getParamsetDescription(channel, type, remoteID, remoteChannel);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getParamsetId(std::string serialNumber, uint32_t channel, RPC::ParameterSet::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel)
{
	try
	{
		if(serialNumber == _serialNumber)
		{
			if(channel > 0) return RPC::RPCVariable::createError(-2, "Unknown channel.");
			if(type != RPC::ParameterSet::Type::Enum::master) return RPC::RPCVariable::createError(-3, "Unknown parameter set.");
			return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(std::string("rf_homegear_central_master")));
		}
		else
		{
			std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
			uint64_t remoteID = 0;
			if(!remoteSerialNumber.empty())
			{
				std::shared_ptr<HMWiredPeer> remotePeer(getPeer(remoteSerialNumber));
				if(remotePeer) remoteID = remotePeer->getID();
			}
			if(peer) return peer->getParamsetId(channel, type, remoteID, remoteChannel);
			return RPC::RPCVariable::createError(-2, "Unknown device.");
		}
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getParamsetId(uint64_t peerID, uint32_t channel, RPC::ParameterSet::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(peerID));
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
		return peer->getParamsetId(channel, type, remoteID, remoteChannel);
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getPeerID(int32_t address)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer = getPeer(address);
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable((int32_t)peer->getID()));
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getPeerID(std::string serialNumber)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer = getPeer(serialNumber);
		if(!peer) return RPC::RPCVariable::createError(-2, "Unknown device.");
		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable((int32_t)peer->getID()));
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getServiceMessages(bool returnID)
{
	try
	{
		std::vector<std::shared_ptr<HMWiredPeer>> peers;
		//Copy all peers first, because getServiceMessages takes very long and we don't want to lock _peersMutex too long
		_peersMutex.lock();
		for(std::map<uint64_t, std::shared_ptr<HMWiredPeer>>::iterator i = _peersByID.begin(); i != _peersByID.end(); ++i)
		{
			peers.push_back(i->second);
		}
		_peersMutex.unlock();

		std::shared_ptr<RPC::RPCVariable> serviceMessages(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
		for(std::vector<std::shared_ptr<HMWiredPeer>>::iterator i = peers.begin(); i != peers.end(); ++i)
		{
			if(!*i) continue;
			//getServiceMessages really needs a lot of ressources, so wait a little bit after each device
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
			std::shared_ptr<RPC::RPCVariable> messages = (*i)->getServiceMessages(returnID);
			if(!messages->arrayValue->empty()) serviceMessages->arrayValue->insert(serviceMessages->arrayValue->end(), messages->arrayValue->begin(), messages->arrayValue->end());
		}
		return serviceMessages;
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _peersMutex.unlock();
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getValue(std::string serialNumber, uint32_t channel, std::string valueKey)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
		if(peer) return peer->getValue(channel, valueKey);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::getValue(uint64_t id, uint32_t channel, std::string valueKey)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(id));
		if(peer) return peer->getValue(channel, valueKey);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::listDevices()
{
	return listDevices(std::shared_ptr<std::map<uint64_t, int32_t>>());
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::listDevices(std::shared_ptr<std::map<uint64_t, int32_t>> knownDevices)
{
	try
	{
		std::shared_ptr<RPC::RPCVariable> array(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));

		std::vector<std::shared_ptr<HMWiredPeer>> peers;
		//Copy all peers first, because listDevices takes very long and we don't want to lock _peersMutex too long
		_peersMutex.lock();
		for(std::map<uint64_t, std::shared_ptr<HMWiredPeer>>::iterator i = _peersByID.begin(); i != _peersByID.end(); ++i)
		{
			if(knownDevices && knownDevices->find(i->first) != knownDevices->end()) continue; //only add unknown devices
			peers.push_back(i->second);
		}
		_peersMutex.unlock();

		for(std::vector<std::shared_ptr<HMWiredPeer>>::iterator i = peers.begin(); i != peers.end(); ++i)
		{
			//listDevices really needs a lot of resources, so wait a little bit after each device
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
			std::shared_ptr<std::vector<std::shared_ptr<RPC::RPCVariable>>> descriptions = (*i)->getDeviceDescription();
			if(!descriptions) continue;
			for(std::vector<std::shared_ptr<RPC::RPCVariable>>::iterator j = descriptions->begin(); j != descriptions->end(); ++j)
			{
				array->arrayValue->push_back(*j);
			}
		}

		return array;
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _peersMutex.unlock();
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::putParamset(std::string serialNumber, int32_t channel, RPC::ParameterSet::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, std::shared_ptr<RPC::RPCVariable> paramset)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
		uint64_t remoteID = 0;
		if(!remoteSerialNumber.empty())
		{
			std::shared_ptr<HMWiredPeer> remotePeer(getPeer(remoteSerialNumber));
			if(!remotePeer) return RPC::RPCVariable::createError(-3, "Remote peer is unknown.");
			remoteID = remotePeer->getID();
		}
		if(peer) return peer->putParamset(channel, type, remoteID, remoteChannel, paramset);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::putParamset(uint64_t peerID, int32_t channel, RPC::ParameterSet::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, std::shared_ptr<RPC::RPCVariable> paramset)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(peerID));
		if(peer) return peer->putParamset(channel, type, remoteID, remoteChannel, paramset);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::removeLink(std::string senderSerialNumber, int32_t senderChannelIndex, std::string receiverSerialNumber, int32_t receiverChannelIndex)
{
	try
	{
		if(senderSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<HMWiredPeer> sender = getPeer(senderSerialNumber);
		std::shared_ptr<HMWiredPeer> receiver = getPeer(receiverSerialNumber);
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		return removeLink(sender->getID(), senderChannelIndex, receiver->getID(), receiverChannelIndex);
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::removeLink(uint64_t senderID, int32_t senderChannelIndex, uint64_t receiverID, int32_t receiverChannelIndex)
{
	try
	{
		if(senderID == 0) return RPC::RPCVariable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return RPC::RPCVariable::createError(-2, "Receiver id is not set.");
		std::shared_ptr<HMWiredPeer> sender = getPeer(senderID);
		std::shared_ptr<HMWiredPeer> receiver = getPeer(receiverID);
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		if(senderChannelIndex < 0) senderChannelIndex = 0;
		if(receiverChannelIndex < 0) receiverChannelIndex = 0;
		if(sender->rpcDevice->channels.find(senderChannelIndex) == sender->rpcDevice->channels.end()) return RPC::RPCVariable::createError(-2, "Sender channel not found.");
		if(receiver->rpcDevice->channels.find(receiverChannelIndex) == receiver->rpcDevice->channels.end()) return RPC::RPCVariable::createError(-2, "Receiver channel not found.");
		if(!sender->getPeer(senderChannelIndex, receiver->getID()) && !receiver->getPeer(receiverChannelIndex, sender->getID())) return RPC::RPCVariable::createError(-6, "Devices are not paired to each other.");

		sender->removePeer(senderChannelIndex, receiver->getID(), receiverChannelIndex);
		receiver->removePeer(receiverChannelIndex, sender->getID(), senderChannelIndex);

		GD::rpcClient.broadcastUpdateDevice(sender->getID(), senderChannelIndex, sender->getSerialNumber() + ":" + std::to_string(senderChannelIndex), RPC::Client::Hint::Enum::updateHintLinks);
		GD::rpcClient.broadcastUpdateDevice(receiver->getID(), receiverChannelIndex, receiver->getSerialNumber() + ":" + std::to_string(receiverChannelIndex), RPC::Client::Hint::Enum::updateHintLinks);

		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::searchDevices()
{
	try
	{
		lockBus();
		_pairing = true;

		std::vector<int32_t> newDevices;
		int32_t addressMask = 0;
		bool backwards = false;
		int32_t address = 0;
		int32_t address2 = 0;
		int64_t time = 0;
		std::shared_ptr<HMWired::HMWiredPacket> receivedPacket;
		int32_t retries = 0;
		int32_t responseDelay = GD::physicalDevices.get(DeviceFamilies::HomeMaticWired)->responseDelay();
		std::pair<uint32_t, std::shared_ptr<HMWiredPacket>> packet;
		while(true)
		{
			std::vector<uint8_t> payload;
			if(packet.second && packet.second->addressMask() == addressMask && packet.second->destinationAddress() == address)
			{
				if(packet.first < 3) packet.first++;
				else
				{
					Output::printError("Event: Prevented deadlock while searching for HomeMatic Wired devices.");
					address++;
					backwards = true;
				}
			}
			else
			{
				packet.first = 0;
				packet.second.reset(new HMWiredPacket(HMWiredPacketType::discovery, 0, address, false, 0, 0, addressMask, payload));
			}
			time = HelperFunctions::getTime();
			sendPacket(packet.second, false);

			int32_t i = 0;
			for(i = 0; i < 2; i++)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(responseDelay));
				receivedPacket = _receivedPackets.get(0);
				if(receivedPacket && receivedPacket->timeReceived() >= time && receivedPacket->type() == HMWiredPacketType::discoveryResponse)
				{
					retries = 0;
					if(addressMask < 31)
					{
						backwards = false;
						addressMask++;
					}
					else
					{
						Output::printMessage("Peer found with address 0x" + HelperFunctions::getHexString(address, 8));
						newDevices.push_back(address);
						backwards = true;
						address++;
						address2 = address;
						int32_t shifts = 0;
						while(!(address2 & 1))
						{
							address2 >>= 1;
							addressMask--;
							shifts++;
						}
						address = address2 << shifts;
					}
					break;
				}
			}
			if(i == 2)
			{
				if(retries < 2) retries++;
				else
				{
					if(addressMask == 0 && (address & 0x80000000)) break;
					retries = 0;
					if(addressMask == 0) break;
					if(backwards)
					{
						//Example:
						//Input:
						//0x8C      0x00      0d21
						//10001100  00000000  10101
						//Output:
						//90        0x00      0d19
						//10010000  00000000  10011
						address2 = address;
						int32_t shifts = 0;
						while(!(address2 & 1))
						{
							address2 >>= 1;
							shifts++;
						}
						address2++;
						while(!(address2 & 1))
						{
							address2 >>= 1;
							shifts++;
							addressMask--;
						}
						address = address2 << shifts;
					}
					else address |= (1 << (31 - addressMask));
				}
			}
		}

		_pairing = false;
		unlockBus();

		Output::printInfo("Info: Search completed. Found " + std::to_string(newDevices.size()) + " devices.");
		std::vector<std::shared_ptr<HMWiredPeer>> newPeers;
		for(std::vector<int32_t>::iterator i = newDevices.begin(); i != newDevices.end(); ++i)
		{
			if(getPeer(*i)) continue;

			//Get device type:
			std::shared_ptr<HMWiredPacket> response = getResponse(0x68, *i, true);
			if(!response || response->payload()->size() != 2)
			{
				Output::printError("Error: HomeMatic Wired Central: Could not pair device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". Device type request failed.");
				continue;
			}
			int32_t deviceType = (response->payload()->at(0) << 8) + response->payload()->at(1);

			//Get firmware version:
			response = getResponse(0x76, *i);
			if(!response || response->payload()->size() != 2)
			{
				Output::printError("Error: HomeMatic Wired Central: Could not pair device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". Firmware version request failed.");
				continue;
			}
			int32_t firmwareVersion = (response->payload()->at(0) << 8) + response->payload()->at(1);

			//Get serial number:
			response = getResponse(0x6E, *i);
			if(!response || response->payload()->empty())
			{
				Output::printError("Error: HomeMatic Wired Central: Could not pair device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". Serial number request failed.");
				continue;
			}
			std::string serialNumber(&response->payload()->at(0), &response->payload()->at(0) + response->payload()->size());

			std::shared_ptr<HMWiredPeer> peer = createPeer(*i, firmwareVersion, LogicalDeviceType(DeviceFamilies::HomeMaticWired, deviceType), serialNumber, true);
			if(!peer)
			{
				Output::printError("Error: HomeMatic Wired Central: Could not pair device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". No matching XML file was found.");
				continue;
			}
			peer->initializeCentralConfig();

			peer->binaryConfig[0].data = readEEPROM(*i, 0);
			peer->saveParameter(peer->binaryConfig[0].databaseID, 0, peer->binaryConfig[0].data);
			if(peer->binaryConfig[0].data.size() != 0x10)
			{
				Output::printError("Error: HomeMatic Wired Central: Could not pair device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". Could not read master config from EEPROM.");
				continue;
			}

			for(std::vector<std::shared_ptr<RPC::Parameter>>::iterator j = peer->rpcDevice->parameterSet->parameters.begin(); j != peer->rpcDevice->parameterSet->parameters.end(); ++j)
			{
				if((*j)->logicalParameter->enforce)
				{
					std::vector<uint8_t> enforceValue = (*j)->convertToPacket((*j)->logicalParameter->getEnforceValue());
					peer->setConfigParameter((*j)->physicalParameter->address.index, (*j)->physicalParameter->size, enforceValue);
				}
			}

			writeEEPROM(*i, 0, peer->binaryConfig[0].data);

			//Read all config
			std::vector<uint8_t> command({0x45, 0, 0, 0x10, 0x40}); //Request used EEPROM blocks; start address 0x0000, block size 0x10, blocks 0x40
			response = getResponse(command, *i);
			if(!response || response->payload()->empty() || response->payload()->size() != 12 || response->payload()->at(0) != 0x65 || response->payload()->at(1) != 0 || response->payload()->at(2) != 0 || response->payload()->at(3) != 0x10)
			{
				Output::printError("Error: HomeMatic Wired Central: Could not pair device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". Could not determine EEPROM blocks to read.");
				continue;
			}

			int32_t configIndex = 0;
			for(int32_t j = 0; j < 8; j++)
			{
				for(int32_t k = 0; k < 8; k++)
				{
					if(response->payload()->at(j + 4) & (1 << k))
					{
						if(peer->binaryConfig.find(configIndex) == peer->binaryConfig.end())
						{
							peer->binaryConfig[configIndex].data = readEEPROM(peer->getAddress(), configIndex);
							peer->saveParameter(peer->binaryConfig[configIndex].databaseID, configIndex, peer->binaryConfig[configIndex].data);
							if(peer->binaryConfig[configIndex].data.size() != 0x10) Output::printError("Error: HomeMatic Wired Central: Error reading config from device with address 0x" + HelperFunctions::getHexString(*i, 8) + ". Size is not 16 bytes.");
						}
					}
					configIndex += 0x10;
				}
			}
			newPeers.push_back(peer);
			_peersMutex.lock();
			try
			{
				_peers[*i] = peer;
				if(!serialNumber.empty()) _peersBySerial[serialNumber] = peer;
				_peersByID[peer->getID()] = peer;
				peer->setMessageCounter(_messageCounter[*i]);
				_messageCounter.erase(*i);
			}
			catch(const std::exception& ex)
			{
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(Exception& ex)
			{
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			_peersMutex.unlock();
		}

		if(newPeers.size() > 0)
		{
			std::shared_ptr<RPC::RPCVariable> deviceDescriptions(new RPC::RPCVariable(RPC::RPCVariableType::rpcArray));
			for(std::vector<std::shared_ptr<HMWiredPeer>>::iterator i = newPeers.begin(); i != newPeers.end(); ++i)
			{
				(*i)->restoreLinks();
				std::shared_ptr<std::vector<std::shared_ptr<RPC::RPCVariable>>> descriptions = (*i)->getDeviceDescription();
				if(!descriptions) continue;
				for(std::vector<std::shared_ptr<RPC::RPCVariable>>::iterator j = descriptions->begin(); j != descriptions->end(); ++j)
				{
					deviceDescriptions->arrayValue->push_back(*j);
				}
			}
			GD::rpcClient.broadcastNewDevices(deviceDescriptions);
		}
		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable((uint32_t)newPeers.size()));
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	_pairing = false;
	unlockBus();
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::setLinkInfo(std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel, std::string name, std::string description)
{
	try
	{
		if(senderSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return RPC::RPCVariable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<HMWiredPeer> sender(getPeer(senderSerialNumber));
		std::shared_ptr<HMWiredPeer> receiver(getPeer(receiverSerialNumber));
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		std::shared_ptr<RPC::RPCVariable> result1 = sender->setLinkInfo(senderChannel, receiver->getID(), receiverChannel, name, description);
		std::shared_ptr<RPC::RPCVariable> result2 = receiver->setLinkInfo(receiverChannel, sender->getID(), senderChannel, name, description);
		if(result1->errorStruct) return result1;
		if(result2->errorStruct) return result2;
		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::setLinkInfo(uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel, std::string name, std::string description)
{
	try
	{
		if(senderID == 0) return RPC::RPCVariable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return RPC::RPCVariable::createError(-2, "Receiver id is not set.");
		std::shared_ptr<HMWiredPeer> sender(getPeer(senderID));
		std::shared_ptr<HMWiredPeer> receiver(getPeer(receiverID));
		if(!sender) return RPC::RPCVariable::createError(-2, "Sender device not found.");
		if(!receiver) return RPC::RPCVariable::createError(-2, "Receiver device not found.");
		std::shared_ptr<RPC::RPCVariable> result1 = sender->setLinkInfo(senderChannel, receiver->getID(), receiverChannel, name, description);
		std::shared_ptr<RPC::RPCVariable> result2 = receiver->setLinkInfo(receiverChannel, sender->getID(), senderChannel, name, description);
		if(result1->errorStruct) return result1;
		if(result2->errorStruct) return result2;
		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
	}
	catch(const std::exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(Exception& ex)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	catch(...)
	{
		Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
	return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::setValue(std::string serialNumber, uint32_t channel, std::string valueKey, std::shared_ptr<RPC::RPCVariable> value)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(serialNumber));
		if(peer) return peer->setValue(channel, valueKey, value);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::setValue(uint64_t id, uint32_t channel, std::string valueKey, std::shared_ptr<RPC::RPCVariable> value)
{
	try
	{
		std::shared_ptr<HMWiredPeer> peer(getPeer(id));
		if(peer) return peer->setValue(channel, valueKey, value);
		return RPC::RPCVariable::createError(-2, "Unknown device.");
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<RPC::RPCVariable> HMWiredCentral::updateFirmware(std::vector<uint64_t> ids, bool manual)
{
	try
	{
		if(_updateMode || GD::devices.updateInfo.currentDevice > 0) return RPC::RPCVariable::createError(-32500, "Central is already already updating a device. Please wait until the current update is finished.");
		if(_updateFirmwareThread.joinable()) _updateFirmwareThread.join();
		_updateFirmwareThread = std::thread(&HMWiredCentral::updateFirmwares, this, ids);
		return std::shared_ptr<RPC::RPCVariable>(new RPC::RPCVariable(RPC::RPCVariableType::rpcVoid));
	}
	catch(const std::exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return RPC::RPCVariable::createError(-32500, "Unknown application error.");
}
} /* namespace HMWired */
