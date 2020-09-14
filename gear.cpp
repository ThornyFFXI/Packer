#include "Packer.h"

void packer::gear(const char* filename)
{
    string profile(filename);

    if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check for absolute path
    {
        profile = string(m_AshitaCore->GetInstallPath()) + "config\\ashitacast\\" + string(filename);
        if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check for filename in packer's config directory
        {
            if (endsWith(profile, ".xml"))
            {
                pOutput->error_f("Gear failed.  File not found.  [$H%s$R]", profile.c_str());
                return;
            }
            profile += ".xml";
            if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check if user omitted the .xml
            {
                pOutput->error_f("Gear failed.  File not found.  [$H%s$R]", profile.c_str());
                return;
            }
        }
    }

    char* buffer = NULL;
    xml_document<>* document = pSettings->LoadXml(filename, buffer);
    if (document != NULL)
    {
        gear(document);
        delete[] buffer;
        delete document;
    }
    else
    {
        sendEventResponse("FAILED");
        mEventState.eventIsActive = false;
    }
}

void packer::gear(xml_document<>* document)
{
    if (!mServer.inventoryLoaded)
    {
        pOutput->error("Could not perform gear.  Inventory has not finished loading since you last zoned.");
        sendEventResponse("FAILED");
        mEventState.eventIsActive = false;
        return;
    }

    if (!parseAshitacastXml(document, &mGear.equipment, &mGear.other))
    {
        sendEventResponse("FAILED");
        mEventState.eventIsActive = false;
        return;
    }

    //Set flag so that we know inventory has to be reparsed.
    mGear.reparse     = true;

    //Set flag to block user item movements and start packer item movements.
    mGear.active = true;

    //Notify the plugin that requested the event that it has began.
    sendEventResponse("STARTED");
}