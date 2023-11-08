#include "WiFi.h"
#include "AsyncUDP.h"
#include <esp32cam.h>
#include <esp_camera.h>

const char* ssid = "********";
const char* password = "********";
const char* passcode = "********";
uint8_t* PASSCODE;

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto hiRes = esp32cam::Resolution::find(800, 600);
AsyncUDP udp;

void serveJpg(AsyncUDPPacket packet)
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    packet.printf("ERROR");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(), static_cast<int>(frame->size()));

  frame->writeTo(packet);
}

void handleJpgLo(AsyncUDPPacket packet)
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg(packet);
}

void handleJpgHi(AsyncUDPPacket packet)
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg(packet);
}

void setup()
{
    Serial.begin(115200);
    {
      using namespace esp32cam;
      Config cfg;
      cfg.setPins(pins::AiThinker);
      cfg.setResolution(hiRes);
      cfg.setBufferCount(3);
      cfg.setJpeg(85);

      bool ok = Camera.begin(cfg);
      Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
    }

    sensor_t* sensor = esp_camera_sensor_get();
    sensor->set_xclk(sensor, 0, 10000000); // 10Mhz seems to work well

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed");
        while(1) {
            delay(1000);
        }
    }

    PASSCODE = (uint8_t*) malloc(strlen(passcode)*sizeof(uint8_t));
    for (int i = 0; i < strlen(passcode); i++) {
      PASSCODE[i] = passcode[i];
    }

    if(udp.listen(5030)) {
        Serial.print("UDP Listening on IP: ");
        Serial.println(WiFi.localIP());
        udp.onPacket([](AsyncUDPPacket packet) {
            Serial.print("UDP Packet Type: ");
            Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
            Serial.print(", From: ");
            Serial.print(packet.remoteIP());
            Serial.print(":");
            Serial.print(packet.remotePort());
            Serial.print(", To: ");
            Serial.print(packet.localIP());
            Serial.print(":");
            Serial.print(packet.localPort());
            Serial.print(", Length: ");
            Serial.print(packet.length());
            Serial.print(", Data: ");
            Serial.write(packet.data(), packet.length());
            Serial.println();
            //reply to the client
            if (packet.length() == strlen(passcode)) {
              uint8_t* data = packet.data();
              bool correct = true;
              for (int i = 0; i < strlen(passcode); i++) {
                if (data[i] != PASSCODE[i]) {
                  correct = false;
                  break;
                }
              }

              if (correct)
                handleJpgLo(packet);
              else
                packet.printf("ERROR");
            } else
              packet.printf("ERROR");
        });
    }
}

void loop()
{
}
