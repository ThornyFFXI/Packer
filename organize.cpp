#include "Packer.h"

void packer::organize()
{    
    if (!mServer.inventoryLoaded)
    {
        pOutput->error("Could not perform organize.  Inventory has not finished loading since you last zoned.");
        sendEventResponse("FAILED");
        mEventState.eventIsActive = false;
        return;
    }

    //Set flag so that we know inventory has to be reparsed.
    mGear.reparse = true;

    //Set flag to block user item movements and start packer item movements.
    mGear.active = true;

    //Notify the plugin that requested the event that it has began.
    sendEventResponse("STARTED");
}