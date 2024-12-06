#include "raylib.h"
#include <enet/enet.h>
#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <format>
#include <cassert>

class Client {
 public:

  Client(std::string lobbyServerHost, int lobbyServerPort) : m_isGameServerStarted(false), m_isConnected(false) {
    m_client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (!m_client)
    {
      printf("Cannot create ENet server\n");
      exit(1);
    }

    ENetAddress address;
    enet_address_set_host(&address, lobbyServerHost.c_str());
    address.port = lobbyServerPort;
    m_lobbyPeer = enet_host_connect(m_client, &address, 2, 0);
    if (!m_lobbyPeer)
    {
      printf("Cannot to lobby");
      exit(1);
    }
  }

  ~Client() {
    enet_host_destroy(m_client);
  }

  void ProcessMessages() {
    ENetEvent event;
    while (enet_host_service(m_client, &event, 10) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        ProcessConnectEvent(event);
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        ProcessReceiveEvent(event);
        break;
      default:
        break;
      };
    }
  }

  void SendStartMessage() {
    const char *message = "start";
    ENetPacket *packet = enet_packet_create(message, strlen(message) + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(m_lobbyPeer, 0, packet);
  }

  void SendPosition(float posx, float posy) {
    std::string message = std::format("pos {} {}", posx, posy);
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(m_gamePeer, 1, packet);
  }

  bool hasLobbyConnection() {
    return m_isConnected;
  }

  bool hasGameConnection() {
    return m_gamePeer;
  }

  std::string GetPingInfoText() {
    return m_pingInfoText;
  }

 private:

  void ProcessConnectEvent(const ENetEvent& event) {
    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
    m_isConnected = true;
  }

  void ProcessReceiveEvent(const ENetEvent& event) {
    printf("Packet received from %x:%u '%s'\n", event.peer->address.host, event.peer->address.port, event.packet->data);

    const char* message = reinterpret_cast<char*>(event.packet->data);
    std::stringstream ss(message);  // mb slow
    std::string messageType;
    ss >> messageType;
    if (!hasGameConnection() && messageType == "go") {
      std::string host;
      int port;
      ss >> host >> port;
      ConnectToGameServer(host, port);
    } else if (messageType == "List") {
      m_pingInfoText = message;
    }
    enet_packet_destroy(event.packet);
  }

  void ConnectToGameServer(std::string host, int port) {
    ENetAddress address;
    enet_address_set_host(&address, host.c_str());
    address.port = port;
    m_gamePeer = enet_host_connect(m_client, &address, 2, 0);
    assert("localhost" == host);
    if (!m_gamePeer)
    {
      std::cout << host << ' ' << port << std::endl;
      printf("Cannot connect to game server");
      exit(1);
    }
  }

  ENetHost* m_client;
  ENetPeer *m_lobbyPeer;
  ENetPeer *m_gamePeer = nullptr;
  bool m_isConnected;
  bool m_isGameServerStarted;
  std::string m_pingInfoText;
};

int main(int argc, const char **argv)
{
  int width = 800;
  int height = 600;
  InitWindow(width, height, "w6 AI MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }
  Client client("localhost", 10887);

  uint32_t timeStart = enet_time_get();
  uint32_t lastSendTime = timeStart;
  float posx = 0.f;
  float posy = 0.f;
  float lastPosx = 1.f;
  float lastPosy = 1.f;
  while (!WindowShouldClose())
  {
    const float dt = GetFrameTime();
    client.ProcessMessages();

    bool enter = IsKeyPressed(KEY_ENTER);
    if (client.hasLobbyConnection())
    {
      uint32_t curTime = enet_time_get();
      if (enter) {
        client.SendStartMessage();
      }
    }
    bool left = IsKeyDown(KEY_LEFT);
    bool right = IsKeyDown(KEY_RIGHT);
    bool up = IsKeyDown(KEY_UP);
    bool down = IsKeyDown(KEY_DOWN);
    constexpr float spd = 10.f;
    posx += ((left ? -1.f : 0.f) + (right ? 1.f : 0.f)) * dt * spd;
    posy += ((up ? -1.f : 0.f) + (down ? 1.f : 0.f)) * dt * spd;

    if (client.hasGameConnection())
    {
      uint32_t curTime = enet_time_get();
      if (curTime - lastSendTime > 100 && (lastPosx != posx || lastPosy != posy))
      {
        lastPosx = posx;
        lastPosy = posy;
        lastSendTime = curTime;
        client.SendPosition(posx, posy);
      }
    }

    BeginDrawing();
      ClearBackground(BLACK);
      DrawText(TextFormat("Current status: %s", "unknown"), 20, 20, 20, WHITE);
      DrawText(TextFormat("My position: (%d, %d)", (int)posx, (int)posy), 20, 40, 20, WHITE);
      DrawText(client.GetPingInfoText().c_str(), 20, 60, 20, WHITE);
    EndDrawing();
  }

  atexit(enet_deinitialize);
  return 0;
}
