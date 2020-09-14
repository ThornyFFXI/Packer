#include "packer.h"

#pragma comment(lib, "psapi.lib")
#include <psapi.h>

__declspec(dllexport) IPlugin* __stdcall expCreatePlugin(const char* args)
{
    UNREFERENCED_PARAMETER(args);

    return (IPlugin*)(new packer());
}

__declspec(dllexport) double __stdcall expGetInterfaceVersion(void)
{
    return ASHITA_INTERFACE_VERSION;
}

bool packer::Initialize(IAshitaCore* core, ILogManager* logger, const uint32_t id)
{
    m_AshitaCore = core;
    m_LogManager = logger;
    m_PluginId = id;

    MODULEINFO mod = { 0 };
    ::GetModuleInformation(::GetCurrentProcess(), ::GetModuleHandle("FFXiMain.dll"), &mod, sizeof(MODULEINFO));
    pWardrobe   = Ashita::Memory::FindPattern((uintptr_t)mod.lpBaseOfDll, (uintptr_t)mod.SizeOfImage, "A1????????568BF1578B88????????C1E902F6C101", 0, 0);
    pZoneFlags = Ashita::Memory::FindPattern((uintptr_t)mod.lpBaseOfDll, (uintptr_t)mod.SizeOfImage, "8B8C24040100008B90????????0BD18990????????8B15????????8B82", 0, 0);

    if (pZoneFlags == 0)
    {
        m_AshitaCore->GetChatManager()->Write(0, false, "Packer: Zone flag signature scan failed.");
        return false;
    }

    pZoneOffset = Read32(pZoneFlags, 0x09);
    if (pZoneOffset == 0)
    {
        m_AshitaCore->GetChatManager()->Write(0, false, "Packer: Zone flag offset not found.");
        return false;
    }

    pZoneFlags = Read32(pZoneFlags, 0x17);
    if (pZoneFlags == 0)
    {
        m_AshitaCore->GetChatManager()->Write(0, false, "Packer: Zone flag sub pointer not found.");
        return false;
    }

    pOutput   = new OutputHelpers(core, logger, this->GetName());
    pSettings = new SettingsHelper(core, pOutput, this->GetName());
    pPacket   = new safePacketInjector(core->GetPacketManager());
    pInv      = m_AshitaCore->GetMemoryManager()->GetInventory();

    initAugmentData();

    if (m_AshitaCore->GetMemoryManager()->GetParty()->GetMemberIsActive(0))
    {
        mServer.charName = m_AshitaCore->GetMemoryManager()->GetParty()->GetMemberName(0);
        loadDefaultProfile(false);
    }

    return true;
}
void packer::Release(void)
{
    delete pPacket;
    delete pSettings;
    delete pOutput;
}
bool packer::HandleCommand(int32_t mode, const char* command, bool injected)
{
    std::vector<string> args;
    int argcount = Ashita::Commands::GetCommandArgs(command, &args);
    if (argcount < 2)
        return false;

    if ((_stricmp(args[0].c_str(), "/pa") == 0) || (_stricmp(args[0].c_str(), "/packer") == 0))
    {
        if ((argcount > 2) && (_stricmp(args[1].c_str(), "validate") == 0))
        {
            //Validate is nice and easy, since it's instant and not threaded.
            validate(args[2].c_str());
        }
        else if ((argcount > 2) && (_stricmp(args[1].c_str(), "gear") == 0))
        {
            stopEvent();

            //Create a new event.
            mEventState.eventIsActive  = true;
            strcpy_s(mEventState.eventName, 256, "GEAR");
            strcpy_s(mEventState.returnEventName, 256, "SELF_EVENT");

            //Trigger actual gear process.
            gear(args[2].c_str());
        }
        else if (_stricmp(args[1].c_str(), "organize") == 0)
        {
            stopEvent();

            //Create a new event.
            mEventState.eventIsActive  = true;
            strcpy_s(mEventState.eventName, 256, "ORGANIZE");
            strcpy_s(mEventState.returnEventName, 256, "SELF_EVENT");

            //Trigger actual organize process.
            organize();
        }
        else if (_stricmp(args[1].c_str(), "stop") == 0)
        {
            stopEvent();
            pOutput->message("Event stopped.");
        }
        else if (_stricmp(args[1].c_str(), "load") == 0)
        {
            if (argcount > 2)
                loadSpecificProfile(args[2]);
            else
                loadDefaultProfile(true);
        }
        else if ((_stricmp(args[1].c_str(), "help") == 0) && (argcount > 2))
        {
            if (_stricmp(args[2].c_str(), "validate") == 0)
            {
                pOutput->message("$H/pa validate [required: Ashitacast Xml Name]");
                pOutput->message("Compare an ashitacast XML to your equippable bags and list which items you do not have in them.");
                pOutput->message("This does not integrate include tags.  If you are using them, please trigger through AC itself.");        
            }
            if (_stricmp(args[2].c_str(), "gear") == 0)
            {
                pOutput->message("$H/pa gear [required: Ashitacast Xml Name]");
                pOutput->message("Automatically move items listed in an ashitacast XML to your equippable bags and organize containers according to your loaded settings XML.");
                pOutput->message("This does not integrate include tags.  If you are using them, please trigger through AC itself.");  
            }
            if (_stricmp(args[2].c_str(), "organize") == 0)
            {
                pOutput->message("$H/pa organize");
                pOutput->message("Organize your containers according to your loaded settings XML.");
            }
            if (_stricmp(args[2].c_str(), "stop") == 0)
            {
                pOutput->message("$H/pa stop");
                pOutput->message("Stop an in-process $HGear$R or $HOrganize$R command.");
            }
            if (_stricmp(args[2].c_str(), "load") == 0)
            {
                pOutput->message("$H/pa load [optional: Packer Config Xml Name]");
                pOutput->message("Load a packer config XML.  If none is specified, will load the default.");
                pOutput->message("This is Ashita/config/packer/charname.xml if it exists, or Ashita/config/packer/default.xml if it does not.");
            }
            else
            {
                pOutput->message("Command List(/pa or /packer)");
                pOutput->message("$H/pa validate [required: Ashitacast Xml Name]");
                pOutput->message("$H/pa gear [required: Ashitacast Xml Name]");
                pOutput->message("$H/pa organize");
                pOutput->message("$H/pa stop");
                pOutput->message("$H/pa load [optional: Packer Config Xml Name]");
            }
        }
        else
        {
            pOutput->message("Command List(/pa or /packer)");
            pOutput->message("$H/pa validate");
            pOutput->message("$H/pa gear [required: Ashitacast Xml Name]");
            pOutput->message("$H/pa organize");
            pOutput->message("$H/pa stop");
            pOutput->message("$H/pa load [optional: Packer Config Xml Name]");
        }

        return true;
    }

    return false;
}
bool packer::HandleIncomingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked)
{
    if (injected)
        return false;

    /*
        Here, we keep a count of how many incoming chunks have come in since the last item assignment packet.  Since our item movements will trigger many item assignments,
        we can use this to determine if the item movements arrived at the server or not.  If we go too long without an item assign packet, we can assume that all of our item
        movements have been dropped.  Similarly, if we go too long without receiving an item assign packet after we've started receiving them, we can assume that all of our
        item movements have been processed.
        Note that updatecontainerinfo tracks if all item movements have completed.
        So, if we reach the timeout after we start receiving item movements we can assume that our packets were not all received or interpreted correctly, and reparse.
    */
    if (Read16(data, 2) != mServer.incomingChunkSequence)
    {
        mServer.incomingChunkSequence = Read16(data, 2);

        if ((!mGear.active) || (mGear.reparse))
        {
            mServer.incomingChunkCount = 0;
            return false;
        }

        auto itemMovementFound = false;
        auto offset = 0;
        auto packet = dataChunk;
        while (offset < sizeChunk)
        {
            const auto size = (*(uint16_t*)packet >> 0x09) * 0x04;
            const auto pid  = (uint16_t)(*(uint16_t*)packet & 0x01FF);
            if ((pid == 0x1E) || (pid == 0x1F) || (pid == 0x20))
                itemMovementFound = true;
            offset += size;
            packet += size;
        }
        
        if (itemMovementFound)
            mServer.incomingChunkCount = 0;
        else
            mServer.incomingChunkCount++;

        if (mServer.incomingChunkCount >= 5)
        {
            //If we've been 5 chunks without seeing an item movement after they started, server likely didn't receive some of our last set of move packets.
            //Just to be safe, we trigger a reparse.
            pOutput->error("No item movements were observed for 5 consecutive chunks.  Reparsing inventory to ensure accuracy.");
            mServer.incomingChunkCount = 0;
            mGear.reparse = true;
        }
    }

    if (id == 0x00A)
    {
        stopEvent();

        string currentName((const char*)data + 0x84);
        if (currentName.length() > 16)
            currentName = currentName.substr(0, 16);

        if (currentName != mServer.charName)
        {
            mServer.charName = currentName;
            loadDefaultProfile(false);
        }

        mServer.inventoryLoaded = false;
    }

    if (id == 0x00B)
    {
        stopEvent();
        mServer.inventoryLoaded = false;
    }

    if (id == 0x1D)
    {
        mServer.inventoryLoaded = true;
    }

    return false;
}
bool packer::HandleOutgoingPacket(uint16_t id, uint32_t size, const uint8_t* data, uint8_t* modified, uint32_t sizeChunk, const uint8_t* dataChunk, bool injected, bool blocked)
{
    //We will be later blocking item movement and sort packets, but we don't want to block our own injected packets.
    if ((injected) && (pPacket->checkOutgoingSelfInjected(id, size, data)))
    {
        mServer.outgoingChunkUsage += size;
        return false;
    }

    /*
    Tracking amount of data used for packet injection to fit as many item movements as possible into a single outgoing chunk.  This ensures it all arrives or none of it does.
    Note that this can be inaccurate if player resizes outgoing packets on a chunk where item movements are being inserted.  However, the incoming chunk fallbacks will guarantee
    that we still continue the process of moving items in the event that injections fall into 2 chunks and only one chunk is lost.

    For fastest results, it is still recommended not to resize outgoing packets while packer is actively moving items.
    */

    if (id == 0x15)
    {
        mServer.outgoingChunkUsage += sizeChunk;

        if (mGear.active)
            processItemMovement();

        mServer.outgoingChunkUsage = 0;
    }

    if (mGear.active)
    {
        for (int x = 0; x < PACKET_MAX; x++)
        {
            if (gBlockedPacketIds[x] == id)
            {
                if (injected)
                    pOutput->error_f("Packet(ID %#03x)[Injected] blocked.  Please do not attempt to modify inventory while Packer is running.", id);
                else
                    pOutput->error_f("Packet(ID %#03x)[Natural] blocked.  Please do not attempt to modify inventory while Packer is running.", id);
                return true;
            }
        }
    }

    return false;
}

void packer::HandleEvent(const char* eventName, const void* eventData, const uint32_t eventSize)
{
    if (_stricmp(eventName, "packer_gear") == 0)
    {
        stopEvent();

        //Create a new event.
        packerPluginEvent_t* event = (packerPluginEvent_t*)eventData;
        mEventState.eventIsActive  = true;
        strcpy_s(mEventState.eventName, 256, "GEAR");
        strcpy_s(mEventState.returnEventName, 256, event->returnEvent);

        //Trigger actual gear process.
        if (event->document != NULL)
            gear(event->document);
        else
            gear(event->fileName);
    }
    if (_stricmp(eventName, "packer_organize") == 0)
    {
        stopEvent();

        //Create a new event.
        packerPluginEvent_t* event = (packerPluginEvent_t*)eventData;
        mEventState.eventIsActive  = true;
        strcpy_s(mEventState.eventName, 256, "ORGANIZE");
        strcpy_s(mEventState.returnEventName, 256, event->returnEvent);

        //Trigger actual organize process.
        organize();
    }
    if (_stricmp(eventName, "packer_validate") == 0)
    {
        //Validate doesn't use a response system, as it is done in place.
        packerPluginEvent_t* event = (packerPluginEvent_t*)eventData;
        if (event->document != NULL)
            validate(event->document);
        else
            validate(event->fileName);
    }
}