#include "protocol.h"
#include <cstring> // memcpy
#include "bitstream.h"
#include <iostream>

void send_join(ENetPeer *peer)
{
  Bitstream bs;
  bs.write(E_CLIENT_TO_SERVER_JOIN);

  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
}

void send_new_entity(ENetPeer *peer, const Entity &ent)
{
  Bitstream bs;
  bs.write(E_SERVER_TO_CLIENT_NEW_ENTITY, ent);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, packet);
}

void send_set_controlled_entity(ENetPeer *peer, uint16_t eid)
{
  Bitstream bs;
  bs.write(E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY, eid);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, packet);
}

void send_entity_state(ENetPeer *peer, uint16_t eid, float x, float y)
{
  Bitstream bs;
  bs.write(E_CLIENT_TO_SERVER_STATE, eid, x, y);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_UNSEQUENCED);

  enet_peer_send(peer, 1, packet);
}

void send_snapshot(ENetPeer *peer, uint16_t eid, float x, float y)
{
  Bitstream bs;
  bs.write(E_SERVER_TO_CLIENT_SNAPSHOT, eid, x, y);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_UNSEQUENCED);
  // std::cout << peer->connectID << std::endl;
  enet_peer_send(peer, 1, packet);
}

MessageType get_packet_type(ENetPacket *packet)
{
  return (MessageType)*packet->data;
}

void deserialize_new_entity(ENetPacket *packet, Entity &ent)
{
  Bitstream bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  bs.skip(sizeof(uint8_t));
  bs.read(ent);
}

void deserialize_set_controlled_entity(ENetPacket *packet, uint16_t &eid)
{
  Bitstream bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  bs.skip(sizeof(uint8_t));
  bs.read(eid);
}

void deserialize_entity_state(ENetPacket *packet, uint16_t &eid, float &x, float &y)
{
  Bitstream bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  bs.skip(sizeof(uint8_t));
  bs.read(eid);
  bs.read(x);
  bs.read(y);
}

void deserialize_snapshot(ENetPacket *packet, uint16_t &eid, float &x, float &y)
{
  Bitstream bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  bs.skip(sizeof(uint8_t));
  bs.read(eid);
  bs.read(x);
  bs.read(y);
  std::cout << eid << ' ' << x << ' ' << y << std::endl;
}