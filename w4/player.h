#pragma once
#include <enet/enet.h>
#include <string>

struct Player {
    ENetPeer* peer;
    std::string name;
    float score;
};