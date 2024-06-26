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
#include <queue>
#include <cassert>

#include "params.h"
static std::vector<Entity> entities;
static std::vector<Entity> entitiesInPrevPast;
static std::vector<Entity> entitiesInPast;
static std::vector<Entity> entitiesInFuture;
static std::vector<std::queue<Entity>> snapshotsHistory;
static std::vector<Entity> localHistory;
static std::unordered_map<uint16_t, size_t> eidToIndexInVectorMap;
static uint16_t my_entity = invalid_entity;
static uint32_t startTime = 0;
static uint32_t currentTick = 0;
static uint32_t startTick;
static uint32_t lastSync;
static uint32_t nDeleted = 0;
static Entity lastMyEntitySnapshot;

void on_new_entity_packet(ENetPacket *packet)
{
  Entity newEntity;
  deserialize_new_entity(packet, newEntity);
  if (eidToIndexInVectorMap.contains(newEntity.eid)) {
    return; // don't need to do anything, we already have entity
  }
  eidToIndexInVectorMap[newEntity.eid] = entities.size();
  entities.push_back(newEntity);
  entitiesInPrevPast.push_back(newEntity);
  entitiesInPast.push_back(newEntity);
  entitiesInFuture.push_back(newEntity);
  std::queue<Entity> newQueue; newQueue.push(newEntity);
  snapshotsHistory.push_back(newQueue);
  if (newEntity.eid == my_entity) {
    localHistory.push_back(newEntity);
    lastMyEntitySnapshot = newEntity;
  }
}

void on_set_controlled_entity(ENetPacket *packet)
{
  deserialize_set_controlled_entity(packet, my_entity);
  if (eidToIndexInVectorMap.contains(my_entity)) {
    localHistory.push_back(entities[eidToIndexInVectorMap[my_entity]]);
    lastMyEntitySnapshot = entities[eidToIndexInVectorMap[my_entity]];
  }
}

void on_snapshot(ENetPacket *packet)
{
  Entity e;
  deserialize_snapshot(packet, e);
  if (e.eid == my_entity) {
    lastMyEntitySnapshot = e;
  } else {
    snapshotsHistory[eidToIndexInVectorMap[e.eid]].push(e);
  }
}

void on_set_time(ENetEvent &event)
{
  uint32_t time;
  deserialize_set_time(event.packet, time);
  constexpr uint32_t offsetInFuture = FIXED_OFFSET + TIME_PER_FRAME; // костыль, без него постоянный рассинхрон с сервером при локальной симуляции
  enet_time_set(time + event.peer->roundTripTime / 2 + offsetInFuture); 
  currentTick = time / fixedDt;
  startTick = currentTick;
  lastSync = currentTick;
}

void setCorrectSnapshotInterval(size_t entityIndexInVector, uint32_t simulateTime) 
{
  Entity &eInPrevPast = entitiesInPrevPast[entityIndexInVector];
  Entity &eInPast = entitiesInPast[entityIndexInVector];
  Entity &eInFuture = entitiesInFuture[entityIndexInVector];
  auto &snapshotsQueue = snapshotsHistory[entityIndexInVector];
  while (snapshotsQueue.size() > 1 && snapshotsQueue.front().tick * fixedDt <= simulateTime) {
    eInPrevPast = eInPast;
    eInPast = snapshotsQueue.front();
    snapshotsQueue.pop();
  }
  eInFuture = snapshotsQueue.front();
}

float quadratic_interpolation(double t0, double t1, double t2, double a, double b, double c, double t) {
  double l0 = (t - t1) * (t - t2) / ((t0 - t1) * (t0 - t2));
  double l1 = (t - t0) * (t - t2) / ((t1 - t0) * (t1 - t2));
  double l2 = (t - t0) * (t - t1) / ((t2 - t0) * (t2 - t1));
  return  a * l0 + b * l1 + c * l2;
}

void interpolateEntity(size_t entityIndexInVector, uint32_t simulateTime)
{
  Entity &e = entities[entityIndexInVector];
  const Entity &eInPrevPast = entitiesInPrevPast[entityIndexInVector];
  const Entity &eInPast = entitiesInPast[entityIndexInVector];
  const Entity &eInFuture = entitiesInFuture[entityIndexInVector];
  if (eInFuture.tick == eInPast.tick) { // чтобы не делило на 0 в следующей формуле. Потенциально баг, потому что они по идее
    return;                             // не должны оказываться равными, но вроде нигде больше это не мешает поэтому тут костыль
  }
  // const float t = clamp(float(simulateTime - eInPast.tick * fixedDt) / ((eInFuture.tick - eInPast.tick) * fixedDt), 0, 1);
  // e.x   = lerp(eInPast.x, eInFuture.x, t);
  // e.y   = lerp(eInPast.y, eInFuture.y, t);
  // e.ori = lerp(eInPast.ori, eInFuture.ori, t); 

  const float t = simulateTime;
  e.x   = quadratic_interpolation(eInPrevPast.tick * fixedDt, eInPast.tick * fixedDt, eInFuture.tick * fixedDt, 
                                  eInPrevPast.x, eInPast.x, eInFuture.x, t);
  e.y   = quadratic_interpolation(eInPrevPast.tick * fixedDt, eInPast.tick * fixedDt, eInFuture.tick * fixedDt, 
                                  eInPrevPast.y, eInPast.y, eInFuture.y, t);
  e.ori = quadratic_interpolation(eInPrevPast.tick * fixedDt, eInPast.tick * fixedDt, eInFuture.tick * fixedDt, 
                                  eInPrevPast.ori, eInPast.ori, eInFuture.ori, t);
}

void setCorrectLocalHistoryInterval(uint32_t simulateTime) 
{
  const int simulateTimeTick = (simulateTime / fixedDt) + 1;
  const int positionInVector = simulateTimeTick - startTick - nDeleted;
  if (positionInVector - 2 >= 0) {
    entitiesInPrevPast[my_entity] = localHistory[positionInVector - 2];
  }
  entitiesInPast[my_entity] = localHistory[positionInVector - 1];
  entitiesInFuture[my_entity] = localHistory[positionInVector];
}

void simulateLocal(uint32_t simulateTime, float thr, float steer) {
  const uint32_t simulateTimeTick = (simulateTime / fixedDt) + 1;
  while (currentTick * fixedDt <= simulateTime) {
    ++currentTick;

    Entity e = localHistory.back();
    if (currentTick >= simulateTimeTick) {
      e.thr = thr; e.steer = steer;
    }
    simulate_entity(e, fixedDt * 0.001f);
    // simulate_entity_cheat(e, fixedDt * 0.001f);

    e.tick = currentTick;
    localHistory.push_back(e);
  }
}

bool isEqual(const Entity &e1, const Entity &e2) 
{
  const bool xEqual     = std::abs(e1.x - e2.x) < 0.01;
  const bool yEqual     = std::abs(e1.y - e2.y) < 0.01;
  const bool oriEqual   = std::abs(e1.ori - e2.ori) < 0.01;
  const bool speedEqual = std::abs(e1.speed - e2.speed) < 0.01;
  const bool thrEqual   = std::abs(e1.thr - e2.thr) < 0.01;
  const bool steerEqual = std::abs(e1.steer - e2.steer) < 0.01;

  return xEqual && yEqual && oriEqual && speedEqual && thrEqual && steerEqual;
}

void clearOldHistory() {
  const uint32_t nZombieStates = (lastSync - startTick) - nDeleted;
  if (nZombieStates * 2 >= localHistory.size()) { // при таком условии получается константная амортизированная сложность удаления элемента
    for (size_t i = 0; i < localHistory.size() - nZombieStates; ++i) {
      localHistory[i] = localHistory[i + nZombieStates];
    }
    localHistory.resize(localHistory.size() - nZombieStates);
    nDeleted += nZombieStates;
  }
}

void adjustHistory() 
{
  const Entity& entityServerState = lastMyEntitySnapshot;
  const size_t indexInHistory = entityServerState.tick - startTick - nDeleted;
  if (indexInHistory >= localHistory.size()) {
    // сервер мог считать тики быстрее чем клиент, поэтому требуемой записи в истории еще могло не быть
    // вроде сейчас это починил, но все равно страшно что упадет, поэтому тут проверка
    return;
  }
  if (lastSync >= entityServerState.tick) {
    // вдруг енет в неправильном порядке (по тикам) пошлет снепшоты и тогда все упадет, 
    // может конечно он следит за порядком, но не хочу разбираться поэтому поставил костыль
    return;
  }
  Entity &entityCurrentState = localHistory[indexInHistory];
  if (!isEqual(entityServerState, entityCurrentState)) 
  {
    std::cout << entityServerState.tick << " adjust\n";
    std::cout << entityServerState.x << ' ' << entityServerState.y << ' ' << entityServerState.speed << ' ' << entityServerState.steer << ' '<< entityServerState.thr << std::endl;
    std::cout << entityCurrentState.x << ' ' << entityCurrentState.y << ' ' << entityCurrentState.speed << ' ' << entityCurrentState.steer << ' '<< entityCurrentState.thr << std::endl;
    assert(entityServerState.tick == entityCurrentState.tick);

    // замена неправильно посчитанного локального состояния на серверное
    entityCurrentState = entityServerState;

    // перенакат последующей истории состояний
    for (size_t i = indexInHistory + 1; i < localHistory.size(); ++i)
    {
      Entity entityNewState = localHistory[i - 1];
      simulate_entity(entityNewState, fixedDt * 0.001f);
      entityNewState.tick += 1;
      localHistory[i] = entityNewState;
    }
  }
  lastSync = entityServerState.tick; // еще lastSync используется в clearOldHistory()
  clearOldHistory();
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

  bool connected = false;
  { 
    // из-за SetTargetFPS получается некорректная синхронизация времени
    // поэтому начальная синхронизация времени происходит тут (в цикле без фиксированного fps)
    // в добавок далее для корректировки времени будет использоваться TIME_PER_FRAME, он тоже нужен чтобы решить проблему с SetTargetFPS
    // глубокий смысл использования константы TIME_PER_FRAME я не понял, просто заметил, что 
    // на сервере была погрешность с клиентом на эту величину

    ENetEvent init_event{};
    bool time_is_set = false;
    while (!time_is_set) {
      enet_host_service(client, &init_event, 1);
      switch (init_event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", init_event.peer->address.host, init_event.peer->address.port);
        send_join(serverPeer);
        connected = true;
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        switch (get_packet_type(init_event.packet))
        {
        case E_SERVER_TO_CLIENT_NEW_ENTITY:
          on_new_entity_packet(init_event.packet);
          break;
        case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
          on_set_controlled_entity(init_event.packet);
          break;
        case E_SERVER_TO_CLIENT_SNAPSHOT:
          on_snapshot(init_event.packet);
          break;
        case E_SERVER_TO_CLIEN_SET_TIME:
          on_set_time(init_event);
          time_is_set = true;
          break;
        };
        enet_packet_destroy(init_event.packet);
        break;
      default:
        break;
      };
    }
  }

  SetTargetFPS(FPS);               // Set our game to run at 60 frames-per-second
  while (!WindowShouldClose())
  {
    ENetEvent event;
    while (enet_host_service(client, &event, 1) > 0) // когда timeout == 0, rtt становится около 10, а так 1 (на сервере также установлено)
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
          on_set_time(event);
          break;
        };
        // std::cout << event.peer->roundTripTime << std::endl;
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }

    uint32_t curTime = enet_time_get();

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

      if (my_entity != -1) {
        // Send
        const uint32_t curTimeTick = (curTime / fixedDt) + 1;
        if (currentTick < curTimeTick) {
          send_entity_input(serverPeer, my_entity, thr, steer, curTimeTick);
        }
        // Локальная симмуляция
        simulateLocal(curTime, thr, steer);
        adjustHistory();
        setCorrectLocalHistoryInterval(curTime);
        interpolateEntity(my_entity, curTime);
      }
      
    }

    // Интерполяция позиций остальных игроков
    for (size_t i = 0; i < entities.size(); ++i) {
      if (entities[i].eid != my_entity) {
        const uint32_t offset = SEND_TIMEOUT + FIXED_OFFSET + TIME_PER_FRAME + serverPeer->roundTripTime;
        setCorrectSnapshotInterval(i, curTime - offset);
        interpolateEntity(i, curTime - offset);
      }
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
