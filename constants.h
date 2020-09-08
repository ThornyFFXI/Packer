#ifndef __ASHITA_packerConstants_H_INCLUDED__
#define __ASHITA_packerConstants_H_INCLUDED__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#define CONTAINER_MAX 13
#define SLOT_MAX 16
#define PACKET_MAX 31

#include <string>
using namespace std;

const string gContainerNames[CONTAINER_MAX] =
    {
        "inventory",
        "safe",
        "storage",
        "temporary",
        "locker",
        "satchel",
        "sack",
        "case",
        "wardrobe",
        "safe2",
        "wardrobe2",
        "wardrobe3",
        "wardrobe4"};

const string gSlotNames[SLOT_MAX] = {
    "main",
    "sub",
    "range",
    "ammo",
    "head",
    "body",
    "hands",
    "legs",
    "feet",
    "neck",
    "waist",
    "ear1",
    "ear2",
    "ring1",
    "ring2",
    "back"};

const std::list<int> gNoItemBags = {
    3,
    8,
    10,
    11,
    12};

const uint16_t gBlockedPacketIds[PACKET_MAX] = {
    0x1A, //Action
    0x28, //Drop Item
    0x29, //Move Item
    0x32, //Trade
    0x33, //Trade
    0x34, //Trade
    0x36, //NPC Trade
    0x37, //Use Item
    0x3A, //Sort
    0x41, //Lot
    0x42, //Pass
    0x4D, //Dbox
    0x4E, //AH
    0x50, //Equip
    0x51, //EquipSet
    0x83, //NPC Buy
    0x84, //NPC Sell
    0x85, //NPC Sell
    0x96, //Synth
    0xAA, //Guild Buy
    0xAB, //Guild Buy List
    0xAC, //Guild Sell
    0xAD, //Guild Sell List
    0xC3, //Make Linkshell
    0xC4, //Equip Linkshell
    0xFA, //Furniture
    0xFB, //Furniture
    0xFC, //Plant Flowerpot
    0xFD, //Examine Flowerpot
    0xFE, //Uproot Flowerpot
    0x100 //Job Change
};

#endif