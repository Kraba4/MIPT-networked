// initial skeleton is a clone from https://github.com/jpcy/bgfx-minimal-example
//
#include <functional>
#include "raylib.h"
#include <enet/enet.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include "entity.h"
#include "protocol.h"
#include <iostream>
#include "mathUtils.h"

static std::vector<Entity> entities;
static std::vector<Entity> entitiesInFuture;
static std::unordered_map<uint16_t, size_t> eidToIndexInVectorMap;
static uint16_t my_entity = invalid_entity;

void on_new_entity_packet(ENetPacket *packet)
{
  Entity newEntity;
  deserialize_new_entity(packet, newEntity);
  if (eidToIndexInVectorMap.contains(newEntity.eid)) {
    return; // don't need to do anything, we already have entity
  }
  eidToIndexInVectorMap[newEntity.eid] = entities.size();
  entities.push_back(newEntity);
  entitiesInFuture.push_back(newEntity);
}

void on_set_controlled_entity(ENetPacket *packet)
{
  deserialize_set_controlled_entity(packet, my_entity);
}

void on_snapshot(ENetPacket *packet)
{
  uint16_t eid = invalid_entity;
  float x = 0.f; float y = 0.f; float ori = 0.f; uint32_t time;
  deserialize_snapshot(packet, eid, x, y, ori, time);
  Entity &eInFuture = entitiesInFuture[eidToIndexInVectorMap[eid]];
  eInFuture.x = x;
  eInFuture.y = y;
  eInFuture.ori = ori;
  eInFuture.lastUpdateTime = time;
}

void on_set_time(ENetPacket *packet)
{
  uint32_t newTime;
  deserialize_set_time(packet, newTime);
  enet_time_set(newTime);
}

void simulateByInterpolation(size_t entityIndexInVector, uint32_t simulateTime)
{
  Entity &e = entities[entityIndexInVector];
  Entity &eInFuture = entitiesInFuture[entityIndexInVector];
  if (eInFuture.lastUpdateTime == e.lastUpdateTime) {
    std::cout << "error" << simulateTime << std::endl;
    return;
  }
  float t = clamp(float(simulateTime - e.lastUpdateTime) / (eInFuture.lastUpdateTime - e.lastUpdateTime), 0, 1);
  e.x   = lerp(e.x, eInFuture.x, t);
  e.y   = lerp(e.y, eInFuture.y, t);
  e.ori = lerp(e.ori, eInFuture.ori, t);
  e.lastUpdateTime = simulateTime;
}

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!client)
  {
    printf("Cannot create ENet client\n");
    return 1;
  }

  ENetAddress address;
  enet_address_set_host(&address, "localhost");
  address.port = 10131;

  ENetPeer *serverPeer = enet_host_connect(client, &address, 2, 0);
  if (!serverPeer)
  {
    printf("Cannot connect to server");
    return 1;
  }

  int width = 600;
  int height = 600;

  InitWindow(width, height, "w5 networked MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.rotation = 0.f;
  camera.zoom = 10.f;


  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

  bool connected = false;
  while (!WindowShouldClose())
  {
    float dt = GetFrameTime();
    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        send_join(serverPeer);
        connected = true;
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        switch (get_packet_type(event.packet))
        {
        case E_SERVER_TO_CLIENT_NEW_ENTITY:
          on_new_entity_packet(event.packet);
          break;
        case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
          on_set_controlled_entity(event.packet);
          break;
        case E_SERVER_TO_CLIENT_SNAPSHOT:
          on_snapshot(event.packet);
          break;
        case E_SERVER_TO_CLIEN_SET_TIME:
          on_set_time(event.packet);
          break;
        };
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
    if (my_entity != invalid_entity)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      Entity &e = entities[eidToIndexInVectorMap[my_entity]];

      // Update
      float thr = (up ? 1.f : 0.f) + (down ? -1.f : 0.f);
      float steer = (left ? -1.f : 0.f) + (right ? 1.f : 0.f);

      // Send
      send_entity_input(serverPeer, my_entity, thr, steer);
    }
    uint32_t curTime = enet_time_get();
    for (size_t i = 0; i < entities.size(); ++i) {
      simulateByInterpolation(i, curTime - 100);
    }
    BeginDrawing();
      ClearBackground(GRAY);
      BeginMode2D(camera);
        for (const Entity &e : entities)
        {
          const Rectangle rect = {e.x, e.y, 3.f, 1.f};
          DrawRectanglePro(rect, {0.f, 0.5f}, e.ori * 180.f / PI, GetColor(e.color));
        }

      EndMode2D();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
