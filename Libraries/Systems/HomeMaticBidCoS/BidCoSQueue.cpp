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

#include "BidCoSQueue.h"
#include "../../GD/GD.h"
#include "BidCoSMessage.h"
#include "PendingBidCoSQueues.h"

namespace BidCoS
{
BidCoSQueue::BidCoSQueue() : _queueType(BidCoSQueueType::EMPTY)
{
	_lastPop = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

BidCoSQueue::BidCoSQueue(BidCoSQueueType queueType) : BidCoSQueue()
{
	_queueType = queueType;
}

BidCoSQueue::~BidCoSQueue()
{
	try
	{
		dispose();
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

void BidCoSQueue::serialize(std::vector<uint8_t>& encodedData)
{
	try
	{
		BinaryEncoder encoder;
		_queueMutex.lock();
		if(_queue.size() == 0)
		{
			_queueMutex.unlock();
			return;
		}
		encoder.encodeByte(encodedData, (int32_t)_queueType);
		encoder.encodeInteger(encodedData, _queue.size());
		for(std::list<BidCoSQueueEntry>::iterator i = _queue.begin(); i != _queue.end(); ++i)
		{
			encoder.encodeByte(encodedData, (uint8_t)i->getType());
			encoder.encodeBoolean(encodedData, i->stealthy);
			encoder.encodeBoolean(encodedData, i->forceResend);
			if(!i->getPacket()) encoder.encodeBoolean(encodedData, false);
			else
			{
				encoder.encodeBoolean(encodedData, true);
				std::vector<uint8_t> packet = i->getPacket()->byteArray();
				encoder.encodeByte(encodedData, packet.size());
				encodedData.insert(encodedData.end(), packet.begin(), packet.end());
			}
			std::shared_ptr<BidCoSMessage> message = i->getMessage();
			if(!message)
			{
				encoder.encodeBoolean(encodedData, false);
				continue;
			}
			encoder.encodeBoolean(encodedData, true);
			encoder.encodeByte(encodedData, message->getDirection());
			encoder.encodeByte(encodedData, message->getMessageType());
			std::vector<std::pair<uint32_t, int32_t>>* subtypes = message->getSubtypes();
			encoder.encodeByte(encodedData, subtypes->size());
			for(std::vector<std::pair<uint32_t, int32_t>>::iterator j = subtypes->begin(); j != subtypes->end(); ++j)
			{
				encoder.encodeByte(encodedData, j->first);
				encoder.encodeByte(encodedData, j->second);
			}
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
	_queueMutex.unlock();
}

void BidCoSQueue::unserialize(std::shared_ptr<std::vector<char>> serializedData, HomeMaticDevice* device, uint32_t position)
{
	try
	{
		BinaryDecoder decoder;
		_queueMutex.lock();
		_queueType = (BidCoSQueueType)decoder.decodeByte(serializedData, position);
		uint32_t queueSize = decoder.decodeInteger(serializedData, position);
		for(uint32_t i = 0; i < queueSize; i++)
		{
			_queue.push_back(BidCoSQueueEntry());
			BidCoSQueueEntry* entry = &_queue.back();
			entry->setType((QueueEntryType)decoder.decodeByte(serializedData, position));
			entry->stealthy = decoder.decodeBoolean(serializedData, position);
			entry->forceResend = decoder.decodeBoolean(serializedData, position);
			int32_t packetExists = decoder.decodeBoolean(serializedData, position);
			if(packetExists)
			{
				std::vector<uint8_t> packetData;
				uint32_t dataSize = decoder.decodeByte(serializedData, position);
				if(position + dataSize <= serializedData->size()) packetData.insert(packetData.end(), serializedData->begin() + position, serializedData->begin() + position + dataSize);
				position += dataSize;
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(packetData, false));
				entry->setPacket(packet, false);
			}
			int32_t messageExists = decoder.decodeBoolean(serializedData, position);
			if(!messageExists) continue;
			int32_t direction = decoder.decodeByte(serializedData, position);
			int32_t messageType = decoder.decodeByte(serializedData, position);
			uint32_t subtypeSize = decoder.decodeByte(serializedData, position);
			std::vector<std::pair<uint32_t, int32_t>> subtypes;
			for(uint32_t j = 0; j < subtypeSize; j++)
			{
				subtypes.push_back(std::pair<uint32_t, int32_t>(decoder.decodeByte(serializedData, position), decoder.decodeByte(serializedData, position)));
			}
			entry->setMessage(device->getMessages()->find(direction, messageType, subtypes), false);
		}
	}
	catch(const std::exception& ex)
    {
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	clear();
    	_pendingQueues.reset();
    }
    catch(Exception& ex)
    {
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	clear();
    	_pendingQueues.reset();
    }
    catch(...)
    {
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    	clear();
    	_pendingQueues.reset();
    }
    _queueMutex.unlock();
}

void BidCoSQueue::unserialize_0_0_6(std::string serializedObject, HomeMaticDevice* device)
{
	try
	{
		Output::printDebug("Unserializing queue: " + serializedObject);
		uint32_t pos = 0;
		_queueType = (BidCoSQueueType)std::stoi(serializedObject.substr(pos, 2), 0, 16); pos += 2;
		uint32_t queueSize = std::stol(serializedObject.substr(pos, 4), 0, 16); pos += 4;
		for(uint32_t i = 0; i < queueSize; i++)
		{
			_queueMutex.lock();
			_queue.push_back(BidCoSQueueEntry());
			BidCoSQueueEntry* entry = &_queue.back();
			_queueMutex.unlock();
			entry->setType((QueueEntryType)std::stoi(serializedObject.substr(pos, 1), 0, 16)); pos += 1;
			entry->stealthy = std::stoi(serializedObject.substr(pos, 1)); pos += 1;
			entry->forceResend = std::stoi(serializedObject.substr(pos, 1)); pos += 1;
			int32_t packetExists = std::stoi(serializedObject.substr(pos, 1), 0, 16); pos += 1;
			if(packetExists)
			{
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket());
				uint32_t packetLength = std::stoi(serializedObject.substr(pos, 2), 0, 16); pos += 2;
				std::string packetString(serializedObject.substr(pos, packetLength));
				packet->import(packetString, false); pos += packetLength;
				entry->setPacket(packet, false);
			}
			int32_t messageExists = std::stoi(serializedObject.substr(pos, 1), 0, 16); pos += 1;
			if(!messageExists) continue;
			int32_t direction = std::stoi(serializedObject.substr(pos, 1), 0, 16); pos += 1;
			int32_t messageType = std::stoi(serializedObject.substr(pos, 2), 0, 16); pos += 2;
			uint32_t subtypeSize = std::stoi(serializedObject.substr(pos, 2), 0, 16); pos += 2;
			std::vector<std::pair<uint32_t, int32_t>> subtypes;
			for(uint32_t j = 0; j < subtypeSize; j++)
			{
				subtypes.push_back(std::pair<uint32_t, int32_t>(std::stoi(serializedObject.substr(pos, 2), 0, 16), std::stoi(serializedObject.substr(pos + 2, 2), 0, 16))); pos += 4;
			}
			entry->setMessage(device->getMessages()->find(direction, messageType, subtypes), false);
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	clear();
    	_pendingQueues.reset();
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	clear();
    	_pendingQueues.reset();
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    	clear();
    	_pendingQueues.reset();
    }
}

void BidCoSQueue::dispose()
{
	try
	{
		if(_disposing) return;
		_disposing = true;
		if(_startResendThread.joinable()) _startResendThread.join();
		if(_pushPendingQueueThread.joinable()) _pushPendingQueueThread.join();
        if(_sendThread.joinable()) _sendThread.join();
		stopResendThread();
		stopPopWaitThread();
		_queueMutex.lock();
		_queue.clear();
		_pendingQueues.reset();
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
    _queueMutex.unlock();
}

bool BidCoSQueue::isEmpty()
{
	return _queue.empty() && (!_pendingQueues || _pendingQueues->empty());
}

bool BidCoSQueue::pendingQueuesEmpty()
{
	 return (!_pendingQueues || _pendingQueues->empty());
}

void BidCoSQueue::resend(uint32_t threadId, bool burst)
{
	try
	{
		//Add ~100 milliseconds after popping, otherwise the first resend is 100 ms too early.
		int64_t timeSinceLastPop = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - _lastPop;
		int32_t i = 0;
		std::chrono::milliseconds sleepingTime;
		uint32_t responseDelay = GD::physicalDevices.get(DeviceFamilies::HomeMaticBidCoS)->responseDelay();
		if(timeSinceLastPop < responseDelay)
		{
			sleepingTime = std::chrono::milliseconds((responseDelay - timeSinceLastPop) / 3);
			if(_resendCounter == 0)
			{
				while(!_stopResendThread && i < 3)
				{
					std::this_thread::sleep_for(sleepingTime);
					i++;
				}
			}
		}
		if(_stopResendThread) return;
		if(_resendCounter < 4)
		{
			//Sleep for 200 ms 5 times
			i = 0;
			if(burst)
			{
				longKeepAlive();
				sleepingTime = std::chrono::milliseconds(100);
			}
			else
			{
				keepAlive();
				sleepingTime = std::chrono::milliseconds(25);
			}
			while(!_stopResendThread && i < 8)
			{
				std::this_thread::sleep_for(sleepingTime);
				i++;
			}
		}
		else if(_resendCounter >= 4 && _resendCounter < 6)
		{
			longKeepAlive();
			//Sleep for 500 ms
			i = 0;
			sleepingTime = std::chrono::milliseconds(50);
			while(!_stopResendThread && i < 10)
			{
				std::this_thread::sleep_for(sleepingTime);
				i++;
			}
		}
		else if(_resendCounter >= 6 && _resendCounter < 8)
		{
			longKeepAlive();
			//Sleep for 1000 ms
			i = 0;
			sleepingTime = std::chrono::milliseconds(50);
			while(!_stopResendThread && i < 20)
			{
				std::this_thread::sleep_for(sleepingTime);
				i++;
			}
		}
		else
		{
			longKeepAlive();
			//Sleep for 5000 ms
			i = 0;
			sleepingTime = std::chrono::milliseconds(200);
			while(!_stopResendThread && i < 25)
			{
				std::this_thread::sleep_for(sleepingTime);
				i++;
			}
		}
		if(_stopResendThread) return;

		_queueMutex.lock();
		if(!_queue.empty() && !_stopResendThread)
		{
			if(_stopResendThread)
			{
				_queueMutex.unlock();
				return;
			}
			bool forceResend = _queue.front().forceResend;
			if(!noSending)
			{
				Output::printDebug("Sending from resend thread " + std::to_string(threadId) + " of queue " + std::to_string(id) + ".");
				std::shared_ptr<BidCoSPacket> packet = _queue.front().getPacket();
				if(!packet) return;
				if(_queue.front().getType() == QueueEntryType::MESSAGE)
				{
					Output::printDebug("Invoking outgoing message handler from BidCoSQueue.");
					BidCoSMessage* message = _queue.front().getMessage().get();
					_queueMutex.unlock();
					_sendThreadMutex.lock();
					if(_sendThread.joinable()) _sendThread.join();
					if(_stopResendThread)
					{
						_sendThreadMutex.unlock();
						return;
					}
					_sendThread = std::thread(&BidCoSMessage::invokeMessageHandlerOutgoing, message, packet);
					Threads::setThreadPriority(_sendThread.native_handle(), 45);
					_sendThreadMutex.unlock();
				}
				else
				{
					bool stealthy = _queue.front().stealthy;
					_queueMutex.unlock();
					_sendThreadMutex.lock();
					if(_sendThread.joinable()) _sendThread.join();
					if(_stopResendThread)
					{
						_sendThreadMutex.unlock();
						return;
					}
					_sendThread = std::thread(&BidCoSQueue::send, this, packet, stealthy);
					Threads::setThreadPriority(_sendThread.native_handle(), 45);
					_sendThreadMutex.unlock();
				}
			}
			else _queueMutex.unlock(); //Has to be unlocked before startResendThread
			if(_stopResendThread) return;
			if(_resendCounter < (retries - 2)) //This actually means that the message will be sent three times all together if there is no response
			{
				_resendCounter++;
				if(_startResendThread.joinable()) _startResendThread.join();
				_startResendThread = std::thread(&BidCoSQueue::startResendThread, this, forceResend);
			}
			else _resendCounter = 0;
		}
		else _queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::push(std::shared_ptr<BidCoSPacket> packet, bool stealthy, bool forceResend)
{
	try
	{
		BidCoSQueueEntry entry;
		entry.setPacket(packet, true);
		entry.stealthy = stealthy;
		entry.forceResend = forceResend;
		_queueMutex.lock();
		if(!noSending && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()->getDirection() == DIRECTIONIN)))
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
			_resendCounter = 0;
			if(!noSending)
			{
				_sendThreadMutex.lock();
				if(_sendThread.joinable()) _sendThread.join();
				_sendThread = std::thread(&BidCoSQueue::send, this, entry.getPacket(), entry.stealthy);
				Threads::setThreadPriority(_sendThread.native_handle(), 45);
				_sendThreadMutex.unlock();
				startResendThread(forceResend);
			}
		}
		else
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::push(std::shared_ptr<PendingBidCoSQueues>& pendingQueues)
{
	try
	{
		_queueMutex.lock();
		_pendingQueues = pendingQueues;
		if(_queue.empty())
		{
			 _queueMutex.unlock();
			pushPendingQueue();
		}
		else  _queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		 _queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	 _queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	 _queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }

}

void BidCoSQueue::push(std::shared_ptr<BidCoSQueue> pendingQueue, bool popImmediately, bool clearPendingQueues)
{
	try
	{
		if(!pendingQueue) return;
		_queueMutex.lock();
		if(!_pendingQueues) _pendingQueues.reset(new PendingBidCoSQueues());
		if(clearPendingQueues) _pendingQueues->clear();
		_pendingQueues->push(pendingQueue);
		_queueMutex.unlock();
		pushPendingQueue();
		_queueMutex.lock();
		if(popImmediately)
		{
			_pendingQueues->pop();
			_workingOnPendingQueue = false;
		}
		_queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::push(std::shared_ptr<BidCoSMessage> message, std::shared_ptr<BidCoSPacket> packet, bool forceResend)
{
	try
	{
		if(message->getDirection() != DIRECTIONOUT) Output::printWarning("Warning: Wrong push method used. Packet is not necessary for incoming messages");
		if(message == nullptr) return;
		BidCoSQueueEntry entry;
		entry.setMessage(message, true);
		entry.setPacket(packet, false);
		entry.forceResend = forceResend;
		_queueMutex.lock();
		if(!noSending && entry.getMessage()->getDirection() == DIRECTIONOUT && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()->getDirection() == DIRECTIONIN)))
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
			_resendCounter = 0;
			if(!noSending)
			{
				_sendThreadMutex.lock();
				if(_sendThread.joinable()) _sendThread.join();
				_sendThread = std::thread(&BidCoSMessage::invokeMessageHandlerOutgoing, message.get(), entry.getPacket());
				Threads::setThreadPriority(_sendThread.native_handle(), 45);
				_sendThreadMutex.unlock();
				startResendThread(forceResend);
			}
		}
		else
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::push(std::shared_ptr<BidCoSMessage> message, bool forceResend)
{
	try
	{
		if(message->getDirection() == DIRECTIONOUT) Output::printCritical("Critical: Wrong push method used. Please provide the received packet for outgoing messages");
		if(message == nullptr) return;
		BidCoSQueueEntry entry;
		entry.setMessage(message, true);
		entry.forceResend = forceResend;
		_queueMutex.lock();
		if(!noSending && entry.getMessage()->getDirection() == DIRECTIONOUT && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()->getDirection() == DIRECTIONIN)))
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
			_resendCounter = 0;
			if(!noSending)
			{
				_sendThreadMutex.lock();
				if(_sendThread.joinable()) _sendThread.join();
				_sendThread = std::thread(&BidCoSMessage::invokeMessageHandlerOutgoing, message.get(), entry.getPacket());
				Threads::setThreadPriority(_sendThread.native_handle(), 45);
				_sendThreadMutex.unlock();
				startResendThread(forceResend);
			}
		}
		else
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::pushFront(std::shared_ptr<BidCoSPacket> packet, bool stealthy, bool popBeforePushing, bool forceResend)
{
	try
	{
		keepAlive();
		if(popBeforePushing)
		{
			Output::printDebug("Popping from BidCoSQueue and pushing packet at the front: " + std::to_string(id));
			if(_popWaitThread.joinable()) _stopPopWaitThread = true;
			if(_resendThread.joinable()) _stopResendThread = true;
			_queueMutex.lock();
			_queue.pop_front();
			_queueMutex.unlock();
		}
		BidCoSQueueEntry entry;
		entry.setPacket(packet, true);
		entry.stealthy = stealthy;
		entry.forceResend = forceResend;
		if(!noSending)
		{
			_queueMutex.lock();
			_queue.push_front(entry);
			_queueMutex.unlock();
			_resendCounter = 0;
			if(!noSending)
			{
				_sendThreadMutex.lock();
				if(_sendThread.joinable()) _sendThread.join();
				_sendThread = std::thread(&BidCoSQueue::send, this, entry.getPacket(), entry.stealthy);
				Threads::setThreadPriority(_sendThread.native_handle(), 45);
				_sendThreadMutex.unlock();
				startResendThread(forceResend);
			}
		}
		else
		{
			_queueMutex.lock();
			_queue.push_front(entry);
			_queueMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
        Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::stopPopWaitThread()
{
	try
	{
		_stopPopWaitThread = true;
		if(_popWaitThread.joinable()) _popWaitThread.join();
		_stopPopWaitThread = false;
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

void BidCoSQueue::popWait(uint32_t waitingTime)
{
	try
	{
		stopResendThread();
		stopPopWaitThread();
		_popWaitThread = std::thread(&BidCoSQueue::popWaitThread, this, _popWaitThreadId++, waitingTime);
		Threads::setThreadPriority(_popWaitThread.native_handle(), 45);
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

void BidCoSQueue::popWaitThread(uint32_t threadId, uint32_t waitingTime)
{
	try
	{
		std::chrono::milliseconds sleepingTime(25);
		uint32_t i = 0;
		while(!_stopPopWaitThread && i < waitingTime)
		{
			std::this_thread::sleep_for(sleepingTime);
			i += 25;
		}
		if(!_stopPopWaitThread)
		{
			pop();
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
}

void BidCoSQueue::send(std::shared_ptr<BidCoSPacket> packet, bool stealthy)
{
	try
	{
		if(noSending || _disposing) return;
		if(device) device->sendPacket(packet, stealthy);
		else Output::printError("Error: Device pointer of queue " + std::to_string(id) + " is null.");
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

void BidCoSQueue::stopResendThread()
{
	try
	{
		_stopResendThread = true;
		if(_resendThread.joinable()) _resendThread.join();
		_stopResendThread = false;
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

void BidCoSQueue::startResendThread(bool force)
{
	try
	{
		if(noSending || _disposing) return;
		_queueMutex.lock();
		if(_queue.empty())
		{
			_queueMutex.unlock();
			return;
		}
		uint8_t controlByte = 0;
		if(_queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage())
		{
			controlByte = _queue.front().getMessage()->getControlByte();
		}
		else if(_queue.front().getPacket())
		{
			controlByte = _queue.front().getPacket()->controlByte();
		}
		else throw Exception("Packet or message pointer of BidCoS queue is empty.");

		_queueMutex.unlock();
		if((!(controlByte & 0x02) && (controlByte & 0x20)) || force) //Resend when no response?
		{
			stopResendThread();
			bool burst = controlByte & 0x10;
			_resendThread = std::thread(&BidCoSQueue::resend, this, _resendThreadId++, burst);
			Threads::setThreadPriority(_resendThread.native_handle(), 45);
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::clear()
{
	try
	{
		stopResendThread();
		_queueMutex.lock();
		if(_pendingQueues) _pendingQueues->clear();
		_queue.clear();
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
    _queueMutex.unlock();
}

void BidCoSQueue::sleepAndPushPendingQueue()
{
	try
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(GD::physicalDevices.get(DeviceFamilies::HomeMaticBidCoS)->responseDelay()));
		pushPendingQueue();
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

void BidCoSQueue::pushPendingQueue()
{
	try
	{
		if(_disposing) return;
		_queueMutex.lock();
		if(!_pendingQueues || _pendingQueues->empty())
		{
			_queueMutex.unlock();
			return;
		}
		while(!_pendingQueues->empty() && (!_pendingQueues->front() || _pendingQueues->front()->isEmpty()))
		{
			Output::printDebug("Debug: Empty queue was pushed.");
			_pendingQueues->pop();
		}
		if(_pendingQueues->empty())
		{
			_queueMutex.unlock();
			return;
		}
		std::shared_ptr<BidCoSQueue> queue = _pendingQueues->front();
		_queueMutex.unlock();
		if(!queue) return; //Not really necessary, as the mutex is locked, but I had a segmentation fault in this function, so just to make
		_queueType = queue->getQueueType();
		queueEmptyCallback = queue->queueEmptyCallback;
		callbackParameter = queue->callbackParameter;
		retries = queue->retries;
		for(std::list<BidCoSQueueEntry>::iterator i = queue->getQueue()->begin(); i != queue->getQueue()->end(); ++i)
		{
			if(!noSending && i->getType() == QueueEntryType::MESSAGE && i->getMessage()->getDirection() == DIRECTIONOUT && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()->getDirection() == DIRECTIONIN)))
			{
				_queueMutex.lock();
				_queue.push_back(*i);
				_queueMutex.unlock();
				_resendCounter = 0;
				if(!noSending)
				{
					_sendThreadMutex.lock();
					if(_sendThread.joinable()) _sendThread.join();
					_sendThread = std::thread(&BidCoSMessage::invokeMessageHandlerOutgoing, i->getMessage().get(), i->getPacket());
					_sendThreadMutex.unlock();
					Threads::setThreadPriority(_sendThread.native_handle(), 45);
					startResendThread(i->forceResend);
				}
			}
			else if(!noSending && i->getType() == QueueEntryType::PACKET && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()->getDirection() == DIRECTIONIN)))
			{
				_queueMutex.lock();
				_queue.push_back(*i);
				_queueMutex.unlock();
				_resendCounter = 0;
				if(!noSending)
				{
					_sendThreadMutex.lock();
					if(_sendThread.joinable()) _sendThread.join();
					_sendThread = std::thread(&BidCoSQueue::send, this, i->getPacket(), i->stealthy);
					_sendThreadMutex.unlock();
					Threads::setThreadPriority(_sendThread.native_handle(), 45);
					startResendThread(i->forceResend);
				}
			}
			else
			{
				_queueMutex.lock();
				_queue.push_back(*i);
				_queueMutex.unlock();
			}
		}
		_workingOnPendingQueue = true;
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::keepAlive()
{
	if(_disposing) return;
	if(lastAction) *lastAction = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void BidCoSQueue::longKeepAlive()
{
	if(_disposing) return;
	if(lastAction) *lastAction = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 5000;
}

void BidCoSQueue::nextQueueEntry()
{
	try
	{
		if(_disposing) return;
		_queueMutex.lock();
		if(_queue.empty()) {
			if(queueEmptyCallback && callbackParameter) queueEmptyCallback(callbackParameter);
			if(_workingOnPendingQueue) _pendingQueues->pop();
			if(!_pendingQueues || (_pendingQueues && _pendingQueues->empty()))
			{
				_stopResendThread = true;
				Output::printInfo("Info: Queue " + std::to_string(id) + " is empty and there are no pending queues.");
				_workingOnPendingQueue = false;
				_queueMutex.unlock();
				return;
			}
			else
			{
				_queueMutex.unlock();
				Output::printDebug("Queue " + std::to_string(id) + " is empty. Pushing pending queue...");
				if(_pushPendingQueueThread.joinable()) _pushPendingQueueThread.join();
				_pushPendingQueueThread = std::thread(&BidCoSQueue::pushPendingQueue, this);
				Threads::setThreadPriority(_pushPendingQueueThread.native_handle(), 45);
				return;
			}
		}
		if((_queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()->getDirection() == DIRECTIONOUT) || _queue.front().getType() == QueueEntryType::PACKET)
		{
			_resendCounter = 0;
			if(!noSending)
			{
				bool forceResend = _queue.front().forceResend;
				std::shared_ptr<BidCoSPacket> packet = _queue.front().getPacket();
				if(_queue.front().getType() == QueueEntryType::MESSAGE)
				{
					BidCoSMessage* message = _queue.front().getMessage().get();
					_queueMutex.unlock();
					_sendThreadMutex.lock();
					if(_sendThread.joinable()) _sendThread.join();
					_sendThread = std::thread(&BidCoSMessage::invokeMessageHandlerOutgoing, message, packet);
					_sendThreadMutex.unlock();
				}
				else
				{
					bool stealthy = _queue.front().stealthy;
					_queueMutex.unlock();
					_sendThreadMutex.lock();
					if(_sendThread.joinable()) _sendThread.join();
					_sendThread = std::thread(&BidCoSQueue::send, this, packet, stealthy);
					_sendThreadMutex.unlock();
				}
				Threads::setThreadPriority(_sendThread.native_handle(), 45);
				startResendThread(forceResend);
			}
			else _queueMutex.unlock();
		}
		else _queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::pop()
{
	try
	{
		if(_disposing) return;
		keepAlive();
		Output::printDebug("Popping from BidCoSQueue: " + std::to_string(id));
		if(_popWaitThread.joinable()) _stopPopWaitThread = true;
		if(_resendThread.joinable()) _stopResendThread = true;
		_lastPop = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		_queueMutex.lock();
		if(_queue.empty())
		{
			_queueMutex.unlock();
			return;
		}
		_queue.pop_front();
		if(GD::debugLevel >= 5 && !_queue.empty())
		{
			if(_queue.front().getType() == QueueEntryType::PACKET && _queue.front().getPacket()) Output::printDebug("Packet now at front of queue: " + _queue.front().getPacket()->hexString());
			else if(_queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()) Output::printDebug("Message now at front: Message type: 0x" + HelperFunctions::getHexString(_queue.front().getMessage()->getMessageType()) + " Control byte: 0x" + HelperFunctions::getHexString(_queue.front().getMessage()->getControlByte()));
		}
		_queueMutex.unlock();
		nextQueueEntry();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	Output::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}
}
