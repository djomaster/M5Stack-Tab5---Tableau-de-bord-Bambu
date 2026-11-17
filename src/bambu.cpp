#include "bambu.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "wifi_manager.h"

// Les imprimantes Bambu en mode LAN : MQTT sur TLS (port 8883), utilisateur « bblp », code d'accès en mot de passe.
// Le certificat est auto-signé : il n'est pas vérifié (réseau local uniquement).

static const uint32_t TEST_TIMEOUT_MS = 30000;

static SemaphoreHandle_t g_mutex = nullptr;
static volatile bool g_reload = false;
static volatile bool g_test_req = false;
static volatile bool g_test_stop = false;
static PrinterCfg g_test_cfg;
static JsonDocument g_filter;

struct Lock {
  Lock() { xSemaphoreTake(g_mutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_mutex); }
};

struct Conn {
  PrinterCfg cfg;
  PrinterState st;
  WiFiClientSecure net;
  PubSubClient mqtt;
  uint32_t next_try = 0;
  uint32_t started = 0;
  String topic_report, topic_req;

  int msg_count = 0;

  explicit Conn(const PrinterCfg &c) : cfg(c), mqtt(net) {
    // Le numéro de série est en majuscules ; les sujets MQTT distinguent majuscules et minuscules.
    cfg.serial.trim();
    cfg.serial.toUpperCase();
    cfg.ip.trim();
    net.setInsecure();
    net.setTimeout(6);
    mqtt.setServer(cfg.ip.c_str(), 8883);
    mqtt.setSocketTimeout(6);
    mqtt.setKeepAlive(30);
    // PubSubClient stocke la taille sur 16 bits : 65535 octets au maximum (65536 ou plus échoue sans erreur).
    if (!mqtt.setBufferSize(65535)) Serial.println("MQTT : tampon de 64 Ko non alloue");
    topic_report = "device/" + cfg.serial + "/report";
    topic_req = "device/" + cfg.serial + "/request";
    mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int len) {
      if (msg_count < 3) Serial.printf("MQTT %s : message %u octets sur %s\n", cfg.ip.c_str(), len, topic);
      msg_count++;
      on_message(payload, len);
    });
    started = millis();
  }

  void set_link(Link l) {
    Lock lk;
    st.link = l;
  }

  void on_message(uint8_t *payload, unsigned int len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, len, DeserializationOption::Filter(g_filter));
    if (err) {
      Serial.printf("MQTT : JSON illisible (%s)\n", err.c_str());
      return;
    }
    JsonObject p = doc["print"];
    if (p.isNull()) return;
    Lock lk;
    if (p["gcode_state"].is<const char *>()) st.gcode = p["gcode_state"].as<const char *>();
    if (p["subtask_name"].is<const char *>()) st.file = p["subtask_name"].as<const char *>();
    if (p["mc_percent"].is<int>()) st.percent = p["mc_percent"].as<int>();
    if (p["mc_remaining_time"].is<int>()) st.remaining_min = p["mc_remaining_time"].as<int>();
    if (p["layer_num"].is<int>()) st.layer = p["layer_num"].as<int>();
    if (p["total_layer_num"].is<int>()) st.total_layers = p["total_layer_num"].as<int>();
    if (p["print_error"].is<int>()) st.error = p["print_error"].as<int>();
    if (p["nozzle_temper"].is<float>()) st.nozzle = p["nozzle_temper"].as<float>();
    if (p["bed_temper"].is<float>()) st.bed = p["bed_temper"].as<float>();
    st.got_report = true;
  }

  void step(uint32_t now) {
    if (wifi_state() != WifiState::Connected) {
      if (st.link != Link::Off) set_link(Link::Off);
      return;
    }
    if (mqtt.connected()) {
      mqtt.loop();
      if (!mqtt.connected()) {
        set_link(Link::Connecting);
        next_try = now + 3000;
      }
      return;
    }
    if (now < next_try) return;
    set_link(Link::Connecting);
    String cid = "tab5-" + String((uint32_t)esp_random(), HEX);
    bool ok = mqtt.connect(cid.c_str(), "bblp", cfg.code.c_str());
    if (ok) {
      bool sub_ok = mqtt.subscribe(topic_report.c_str());
      bool pub_ok = mqtt.publish(topic_req.c_str(), "{\"pushing\":{\"sequence_id\":\"1\",\"command\":\"pushall\"}}");
      Serial.printf("MQTT : abonnement %s (%s) = %d, pushall = %d\n", topic_report.c_str(), cfg.ip.c_str(), sub_ok, pub_ok);
      set_link(Link::Connected);
      Serial.printf("MQTT : connecte a %s\n", cfg.ip.c_str());
    } else {
      int rc = mqtt.state();
      Serial.printf("MQTT : echec vers %s (rc=%d)\n", cfg.ip.c_str(), rc);
      bool auth = (rc == 4 || rc == 5);
      set_link(auth ? Link::AuthFailed : Link::Unreachable);
      next_try = millis() + (auth ? 30000 : 10000);
    }
  }
};

static std::vector<Conn *> g_conns;
static Conn *g_test = nullptr;

static void net_task(void *) {
  for (;;) {
    uint32_t now = millis();

    if (g_reload) {
      g_reload = false;
      Lock lk;
      for (auto *c : g_conns) delete c;
      g_conns.clear();
      for (int i = 0; i < printers_count(); i++) g_conns.push_back(new Conn(printers_get(i)));
    }

    if (g_test_stop) {
      g_test_stop = false;
      Lock lk;
      delete g_test;
      g_test = nullptr;
    }
    if (g_test_req) {
      g_test_req = false;
      Lock lk;
      delete g_test;
      g_test = new Conn(g_test_cfg);
    }

    for (size_t i = 0; i < g_conns.size(); i++) g_conns[i]->step(now);

    if (g_test) {
      g_test->step(now);
      if (!g_test->st.got_report && millis() - g_test->started > TEST_TIMEOUT_MS && g_test->st.link != Link::AuthFailed)
        g_test->set_link(Link::Unreachable);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void bambu_start() {
  g_mutex = xSemaphoreCreateMutex();
  JsonObject f = g_filter["print"].to<JsonObject>();
  f["gcode_state"] = true;
  f["subtask_name"] = true;
  f["mc_percent"] = true;
  f["mc_remaining_time"] = true;
  f["layer_num"] = true;
  f["total_layer_num"] = true;
  f["print_error"] = true;
  f["nozzle_temper"] = true;
  f["bed_temper"] = true;
  g_reload = true;
  xTaskCreatePinnedToCore(net_task, "bambu_net", 28672, nullptr, 1, nullptr, 0);
}

void bambu_reload() { g_reload = true; }

bool bambu_state(int i, PrinterState &out) {
  Lock lk;
  if (i < 0 || i >= (int)g_conns.size()) return false;
  out = g_conns[i]->st;
  return true;
}

void bambu_test_start(const PrinterCfg &cfg) {
  {
    Lock lk;
    g_test_cfg = cfg;
  }
  g_test_req = true;
}

bool bambu_test_status(PrinterState &out) {
  Lock lk;
  if (!g_test) return false;
  out = g_test->st;
  return true;
}

void bambu_test_stop() { g_test_stop = true; }
