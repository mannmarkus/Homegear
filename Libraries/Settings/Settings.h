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

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "../HelperFunctions/HelperFunctions.h"
#include "../Threads/Threads.h"

#include <iostream>
#include <string>
#include <map>

class Settings {
public:
	Settings();
	virtual ~Settings() {}
	void load(std::string filename);

	std::string certPath() { return _certPath; }
	std::string keyPath() { return _keyPath;  }
	bool loadDHParamsFromFile() { return _loadDHParamsFromFile; }
	std::string dhParamPath() { return _dhParamPath;  }
	int32_t debugLevel() { return _debugLevel; }
	std::string databasePath() { return _databasePath; }
	bool databaseSynchronous() { return _databaseSynchronous; }
	bool databaseMemoryJournal() { return _databaseMemoryJournal; }
	std::string logfilePath() { return _logfilePath; }
	bool prioritizeThreads() { return _prioritizeThreads; }
	void setPrioritizeThreads(bool value) { _prioritizeThreads = value; }
	uint32_t workerThreadWindow() { return _workerThreadWindow; }
	uint32_t rpcServerThreadPriority() { return _rpcServerThreadPriority; }
	std::string clientSettingsPath() { return _clientSettingsPath; }
	std::string serverSettingsPath() { return _serverSettingsPath; }
	std::string physicalDeviceSettingsPath() { return _physicalDeviceSettingsPath; }
	std::string scriptPath() { return _scriptPath; }
	std::string firmwarePath() { return _firmwarePath; }
	std::map<std::string, bool>& tunnelClients() { return _tunnelClients; }
	std::string gpioPath() { return _gpioPath; }
private:
	std::string _certPath;
	std::string _keyPath;
	bool _loadDHParamsFromFile = true;
	std::string _dhParamPath;
	int32_t _debugLevel = 3;
	std::string _databasePath;
	bool _databaseSynchronous = false;
	bool _databaseMemoryJournal = true;
	std::string _logfilePath;
	bool _prioritizeThreads = true;
	uint32_t _workerThreadWindow = 3000;
	uint32_t _rpcServerThreadPriority = 0;
	std::string _clientSettingsPath;
	std::string _serverSettingsPath;
	std::string _physicalDeviceSettingsPath;
	std::string _scriptPath;
	std::string _firmwarePath;
	std::map<std::string, bool> _tunnelClients;
	std::string _gpioPath;

	void reset();
};

#endif /* SETTINGS_H_ */
