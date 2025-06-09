#ifndef __ASHITA_Thorny_safePacketInjector_H_INCLUDED__
#define __ASHITA_Thorny_safePacketInjector_H_INCLUDED__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "Ashita.h"
#define MAX_PACKET_SIZE 508

class safePacketInjector
{
private:
    uint8_t mIncomingPacketBuffer[MAX_PACKET_SIZE];
    uint16_t mIncomingPacketId;
    uint32_t mIncomingPacketSize;
    uint8_t mOutgoingPacketBuffer[MAX_PACKET_SIZE];
    uint16_t mOutgoingPacketId;
    uint32_t mOutgoingPacketSize;
    IPacketManager* mPacketHandler;

public:
    safePacketInjector(IPacketManager* packetManager)
    {
        mPacketHandler = packetManager;
    }
    ~safePacketInjector(){};

    void addIncomingPacket_s(uint16_t id, uint32_t size, void* data)
    {
        if (size > MAX_PACKET_SIZE)
            return;
      
        memcpy(mIncomingPacketBuffer, data, size);
        mIncomingPacketId   = id;
        mIncomingPacketSize = size;
        mPacketHandler->AddIncomingPacket(id, size, (uint8_t*)data);
        mIncomingPacketSize = 0;
    }
    void addOutgoingPacket_s(uint16_t id, uint32_t size, void* data)
    {
        if (size > MAX_PACKET_SIZE)
            return;

        memcpy(mOutgoingPacketBuffer, data, size);
        mOutgoingPacketId   = id;
        mOutgoingPacketSize = size;
        mPacketHandler->AddOutgoingPacket(id, size, (uint8_t*)data);
        mOutgoingPacketSize = 0;
    }
    bool checkIncomingSelfInjected(uint16_t id, uint32_t size, const uint8_t* data)
    {
        if ((id == mIncomingPacketId) && (size == mIncomingPacketSize) && (memcmp(data + 4, mIncomingPacketBuffer + 4, size - 4) == 0))
        {
            mIncomingPacketSize = 0;
            return true;
        }
        return false;
    }
    bool checkOutgoingSelfInjected(uint16_t id, uint32_t size, const uint8_t* data)
    {
        if ((id == mOutgoingPacketId) && (size == mOutgoingPacketSize) && (memcmp(data + 4, mOutgoingPacketBuffer + 4, size - 4) == 0))
        {
            mOutgoingPacketSize = 0;
            return true;
        }
        return false;
    }
};
#endif