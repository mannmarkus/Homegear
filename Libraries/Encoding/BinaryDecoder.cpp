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

#include "BinaryDecoder.h"

int32_t BinaryDecoder::decodeInteger(std::shared_ptr<std::vector<char>>& encodedData, uint32_t& position)
{
	int32_t integer = 0;
	try
	{
		if(position + 4 > encodedData->size())
		{
			if(position + 1 > encodedData->size()) return 0;
			//IP-Symcon encodes integers as string => Difficult to interpret. This works for numbers up to 3 digits:
			std::string string(&encodedData->at(position), &encodedData->at(encodedData->size() - 1) + 1);
			position = encodedData->size();
			integer = HelperFunctions::getNumber(string);
			return integer;
		}
		HelperFunctions::memcpyBigEndian((char*)&integer, &encodedData->at(position), 4);
		position += 4;
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
	return integer;
}

uint8_t BinaryDecoder::decodeByte(std::shared_ptr<std::vector<char>>& encodedData, uint32_t& position)
{
	int32_t byte = 0;
	try
	{
		if(position + 1 > encodedData->size()) return 0;
		byte = encodedData->at(position);
		position += 1;
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
	return byte;
}

std::string BinaryDecoder::decodeString(std::shared_ptr<std::vector<char>>& encodedData, uint32_t& position)
{
	try
	{
		int32_t stringLength = decodeInteger(encodedData, position);
		if(position + stringLength > encodedData->size() || stringLength == 0) return "";
		std::string string(&encodedData->at(position), &encodedData->at(position) + stringLength);
		position += stringLength;
		return string;
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
    return "";
}

double BinaryDecoder::decodeFloat(std::shared_ptr<std::vector<char>>& encodedData, uint32_t& position)
{
	try
	{
		if(position + 8 > encodedData->size()) return 0;
		int32_t mantissa = 0;
		int32_t exponent = 0;
		HelperFunctions::memcpyBigEndian((char*)&mantissa, &encodedData->at(position), 4);
		position += 4;
		HelperFunctions::memcpyBigEndian((char*)&exponent, &encodedData->at(position), 4);
		position += 4;
		double floatValue = (double)mantissa / 0x40000000;
		floatValue *= std::pow(2, exponent);
		return floatValue;
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
    return 0;
}

bool BinaryDecoder::decodeBoolean(std::shared_ptr<std::vector<char>>& encodedData, uint32_t& position)
{
	try
	{
		if(position + 1 > encodedData->size()) return 0;
		bool boolean = (bool)encodedData->at(position);
		position += 1;
		return boolean;
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
