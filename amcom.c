#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "amcom.h"

/// Start of packet character
const uint8_t  AMCOM_SOP         = 0xA1;
const uint16_t AMCOM_INITIAL_CRC = 0xFFFF;

static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc)
{
	byte ^= (uint8_t)(crc & 0x00ff);
	byte ^= (uint8_t)(byte << 4);
	return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
}


size_t AMCOM_Serialize(uint8_t packetType, const void* payload, size_t payloadSize, uint8_t* destinationBuffer) {
	// TODO
	if(destinationBuffer!=NULL)
    {
        destinationBuffer[0] = AMCOM_SOP;     //SOP
        destinationBuffer[1] = packetType;    //TYPE
        destinationBuffer[2] = payloadSize;   //LENGTH
        
        uint16_t crc=AMCOM_INITIAL_CRC;
        crc = AMCOM_UpdateCRC(packetType,crc);
        crc = AMCOM_UpdateCRC(payloadSize,crc);
        
        if(payload == 0)
        {
            destinationBuffer[3] = crc & 0xFF;    
            destinationBuffer[4] = crc >>0x08;
            return 5;
        }
        else
        {   
            for (size_t s=0; s<payloadSize; s++)
            {
                crc = AMCOM_UpdateCRC(*((uint8_t*)payload+s), crc);
                destinationBuffer[s+5] = *((uint8_t*)payload+s);
            }
            destinationBuffer[3] = crc & 0xFF;
            destinationBuffer[4] = crc >> 0x08;
            return (payloadSize+5);
        }
    }
	return 0;
}

void AMCOM_InitReceiver(AMCOM_Receiver* receiver, AMCOM_PacketHandler packetHandlerCallback, void* userContext) {
		// TODO
        	receiver->packetHandler = packetHandlerCallback;
        	receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
		receiver->userContext = userContext;
		receiver->payloadCounter = 0;
}

void AMCOM_Deserialize(AMCOM_Receiver* receiver, const void* data, size_t dataSize) {
    // TODO
    uint8_t header = 0;
    static uint8_t counter = 0;
    static AMCOM_Packet temporary;
    
    //------------State machine on "if" because "switch case" didn't work----------- 
    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_EMPTY && dataSize > 0)
    {
        if(*((uint8_t *) data+header) == AMCOM_SOP)
        {
            temporary.header.sop = *((uint8_t *) data+header);
            header++;
            dataSize--;
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_SOP;
        }
    }
    
    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_SOP && dataSize > 0)
    {
        if(*((uint8_t *) data+header) != 0x00)
        {
            temporary.header.type = *((uint8_t *) data+header);
            header++;
            dataSize--;
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_TYPE;
        }
    }
    
    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_TYPE && dataSize > 0)
    {
        if(*((uint8_t *) data+header) <= 200)
        {
            temporary.header.length = *((uint8_t *) data+header);
            header++;
            dataSize--;
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_LENGTH;
        }
        else
        {
            receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
        }
    }

    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_LENGTH && dataSize > 0)
    {
        if(*((uint8_t *) data+header) != 0x00)
        {
            temporary.header.crc = *((uint8_t *) data+header);
            header++;
            dataSize--;
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_CRC_LO;
        }
    }
    
    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_CRC_LO && dataSize > 0)
    {
        if(*((uint8_t *) data+header) != 0x00)
        {
            temporary.header.crc |= (uint16_t) (*((uint8_t *) data+header) << 8);
            header++;
            dataSize--;
            
            if(temporary.header.length > 0)
            {
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GETTING_PAYLOAD;
            }
            else
            {
                receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
            }
        }
    }
    
    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GETTING_PAYLOAD && dataSize > 0)
    {
        while(counter != temporary.header.length && dataSize > 0)
        {
            temporary.payload[counter] = *((uint8_t *) data+header);
            header++;
            counter++;
            dataSize--;
        }
            
        if(counter == temporary.header.length)
        {
            receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
        }
    }
    
    if(receiver->receivedPacketState == AMCOM_PACKET_STATE_GOT_WHOLE_PACKET)
    {
        uint16_t crc = AMCOM_INITIAL_CRC;
        crc = AMCOM_UpdateCRC(temporary.header.type, crc);
        crc = AMCOM_UpdateCRC(temporary.header.length, crc);
        
        for(size_t k=0; k<temporary.header.length; k++)
        {
            crc = AMCOM_UpdateCRC(temporary.payload[k], crc);
        }
        if(temporary.header.crc == crc)
        {
            receiver->packetHandler(&temporary, receiver->userContext);
        }
        
        receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
        counter = 0;
    }
}