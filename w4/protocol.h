#pragma once
#include <cstdint>
#include <string>
#include <enet/enet.h>
#include "entity.h"

enum MessageType : uint8_t
{
  E_CLIENT_TO_SERVER_JOIN = 0,
  E_SERVER_TO_CLIENT_NEW_ENTITY,
  E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY,
  E_CLIENT_TO_SERVER_STATE,
  E_SERVER_TO_CLIENT_SNAPSHOT,
  E_SERVER_TO_CLIENT_CHANGE_SIZE,
  E_SERVER_TO_CLIENT_TELEPORT,
  E_SERVER_TO_CLIENT_SCORE
};

void send_join(ENetPeer *peer, const std::string& name);
void send_new_entity(ENetPeer *peer, const Entity &ent);
void send_set_controlled_entity(ENetPeer *peer, uint16_t eid);
void send_entity_state(ENetPeer *peer, uint16_t eid, float x, float y);
void send_snapshot(ENetPeer *peer, uint16_t eid, float x, float y);
void send_change_size(ENetPeer *peer, uint16_t eid, float radius);
void send_teleport(ENetPeer *peer, uint16_t eid, float x, float y);
void send_score(ENetPeer *peer, const std::string& scoreListText);

MessageType get_packet_type(ENetPacket *packet);

void deserialize_join(ENetPacket *packet, std::string& name);
void deserialize_new_entity(ENetPacket *packet, Entity &ent);
void deserialize_set_controlled_entity(ENetPacket *packet, uint16_t &eid);
void deserialize_entity_state(ENetPacket *packet, uint16_t &eid, float &x, float &y);
void deserialize_snapshot(ENetPacket *packet, uint16_t &eid, float &x, float &y);
void deserialize_change_size(ENetPacket *packet, uint16_t &eid, float &radius);
void deserialize_teleport(ENetPacket *packet, uint16_t &eid, float &x, float &y);
void deserialize_score(ENetPacket *peer, std::string& scoreListText);