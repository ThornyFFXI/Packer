#ifndef __ASHITA_Packer_H_INCLUDED__
#define __ASHITA_Packer_H_INCLUDED__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "C:\Ashita 4\plugins\sdk\Ashita.h"
#include "..\common\safePacketInjector.h"
#include "..\common\thirdparty\rapidxml.hpp"
#include "..\common\Output.h"
#include "..\common\Settings.h"
#include "constants.h"
#include "Structs.h"

#define DEFAULT_MOVE_INDEX 82
#define MAX_PACKET_ENCRYPTED_BUFFER_SIZE 1360
#define RBUFP(p, pos) (((uint8_t*)(p)) + (pos))
#define Read8(p, pos) (*(uint8_t*)RBUFP((p), (pos)))
#define Read16(p, pos) (*(uint16_t*)RBUFP((p), (pos)))
#define Read32(p, pos) (*(uint32_t*)RBUFP((p), (pos)))
#define Read64(p, pos) (*(uint64_t*)RBUFP((p), (pos)))
#define ReadFloat(p, pos) (*(float_t*)RBUFP((p), (pos)))

using namespace rapidxml;
using namespace std;

static bool endsWith(const std::string& str, const char* suffix)
{
    if (strlen(suffix) > str.length())
        return false;
    const char* end = str.c_str() + (str.length() - strlen(suffix));
    return (_stricmp(end, suffix) == 0);
}

class packer : IPlugin, Ashita::Threading::Thread
{
private:
    IAshitaCore* m_AshitaCore;
    ILogManager* m_LogManager;
    uint32_t m_PluginId;

public:
    const char* GetName(void) const override
    {
        return "Packer";
    }
    const char* GetAuthor(void) const override
    {
        return "Thorny";
    }
    const char* GetDescription(void) const override
    {
        return "A plugin to automatically gather equipment and organize inventory containers.";
    }
    const char* GetLink(void) const override
    {
        return "https://github.com/Lolwutt/packer";
    }
    double GetVersion(void) const override
    {
        return 1.13f;
    }
    int32_t GetPriority(void) const override
    {
        return 0;
    }
    uint32_t GetFlags(void) const override
    {
        return ((uint32_t)Ashita::PluginFlags::UseCommands | (uint32_t)Ashita::PluginFlags::UsePackets | (uint32_t)Ashita::PluginFlags::UsePluginEvents);
    }

    //Main.cpp
    bool Initialize(IAshitaCore* core, ILogManager* logger, const uint32_t id) override;
    void Release(void) override;
    bool HandleCommand(int32_t mode, const char* command, bool injected) override;
    bool HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked) override;
    bool HandleOutgoingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked) override;
    void HandleEvent(const char* eventName, const void* eventData, const uint32_t eventSize) override;
    
private:
    DWORD pWardrobe;
    DWORD pZoneFlags;
    DWORD pZoneOffset;
    OutputHelpers* pOutput;
    SettingsHelper* pSettings;
    string mCurrentProfile;
    gearState_t mGear;
    serverState_t mServer;
    packerConfig_t mConfig;
    IInventory* pInv;
    safePacketInjector* pPacket;
    vector<augmentResource_t> mAugmentData[3][0x800];

    //profile.cpp
    void clearProfile();
    void loadDefaultProfile(bool forceReload);
    void loadSpecificProfile(string fileName);
    bool writeDefaultProfile(string path);
    bool parseProfileXml(xml_document<>* xmlDocument);
    bool parseAshitacastXml(xml_document<>* xmlDocument, std::list<itemOrder_t>* equipment, std::list<itemOrder_t>* items);
    void combineOrder(list<itemOrder_t>* list, itemOrder_t order);
    void integrateOrder(list<itemOrder_t>* list, itemOrder_t order);

    //helpers.cpp
    int getContainerIndex(const char* text);
    int getSlotIndex(const char* text);
    void waitForThread();
    void stopEvent();
    bool itemMatchesOrder(itemOrder_t order, Ashita::FFXI::item_t* item);
    string getValidateString(itemOrder_t order, bool plural);
    void checkMog();

    //augment.cpp
    void initAugmentData();
    augmentData_t createAugmentData(Ashita::FFXI::item_t* item);

    //gear.cpp
    void gear(const char* filename);
    void gear(xml_document<>* document);
    void gear(GearListEvent_t* gearEvent);

    //organize.cpp
    void organize();

    //validate.cpp
    void validate(const char* filename);
    void validate(xml_document<>* document);
    void validate(GearListEvent_t* validateEvent);
    void validate(std::list<itemOrder_t>* equipment, std::list<itemOrder_t>* items);

    //preparemove.cpp
    void prepareMoveList();
    uint32_t ThreadEntry(void) override;
    void flagMoveItems();
    void createContainerInfo();
    bool verifyContainerInfo();
    bool parseBag(int index, itemOrder_t* order, int targetBag);
    
    //processmove.cpp
    void processItemMovement();
    bool processMoves();
    std::list<itemLoc_t> createFetchList(int container);
    bool tryFetch(std::list<itemLoc_t>* fetchList, int container, bool canStoreEquip, bool canStoreItems);
    bool tryStore(int container, bool force, bool canRetrieve);
    bool processDefrag();

    //domove.cpp
    int doDefrag(IItem* resource, std::list<itemLoc_t> fragments);
    bool moveItem(int container, int index, int count, int destination, IItem* pResource, const char* process);
    void updateItems(pkMoveItem_t* packet, int destination);
    void printDebug(pkMoveItem_t* packet, IItem* resource, const char* process);
};
#endif