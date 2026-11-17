#include "tiger.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "tls_gate.h"
#include "bambu.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <map>
#include <vector>
#include "wifi_manager.h"

// Tiger Studio n'a pas d'API publique : l'application officielle lit une base Firebase (Authentication + Firestore).
// On fait de même avec le compte de l'utilisateur. La clé ci-dessous est la configuration publique de l'application web
// de TigerTag (https://tigertag-cdn.web.app/__/firebase/init.json) ; ce n'est pas un secret.
static const char *FB_KEY = "AIzaSyCkxPTs_Cv0KVLqsZj-UKWWqIY0OtfVpnw";
static const char *FB_PROJECT = "tigertag-connect";
static const char *URL_BRANDS = "https://raw.githubusercontent.com/TigerTag-Project/TigerTag-Studio-Manager/main/assets/db/tigertag/id_brand.json";
static const char *URL_MATERIALS = "https://raw.githubusercontent.com/TigerTag-Project/TigerTag-Studio-Manager/main/assets/db/tigertag/id_material.json";

static const uint32_t SYNC_PERIOD_MS = 30UL * 60UL * 1000UL;
static const uint32_t RETRY_PERIOD_MS = 60UL * 1000UL;
static const size_t MAX_SPOOLS = 400;

static SemaphoreHandle_t g_mutex = nullptr;
struct Lock {
  Lock() { xSemaphoreTake(g_mutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_mutex); }
};

static std::vector<Spool> g_spools;
static std::map<int, String> g_brands, g_materials;
static TigerState g_state = TigerState::SignedOut;
static String g_message;
static int g_rev = 0;

// Compte (le mot de passe n'est jamais conservé : seul le jeton de renouvellement l'est)
static String g_email, g_refresh, g_uid, g_id_token;
static uint32_t g_token_expiry = 0;

// Demandes de l'interface
static volatile bool g_req_login = false, g_req_logout = false, g_req_sync = false;
static String g_req_email, g_req_pass;
static uint32_t g_last_sync = 0, g_next_try = 0;
static bool g_tables_loaded = false;

static void set_state(TigerState s, const String &msg = "") {
  Lock lk;
  g_state = s;
  g_message = msg;
  g_rev++;
}

// ---------------------------------------------------------------------
// Allocateur ArduinoJson en PSRAM : les gros documents JSON ne consomment pas la mémoire interne
struct PsramAlloc : ArduinoJson::Allocator {
  void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
};
static PsramAlloc g_psram;

static int http_json_locked(const String &url, bool post, const String &body, const char *content_type, const char *bearer,
                            JsonDocument &filter, JsonDocument &out);

static int http_json(const String &url, bool post, const String &body, const char *content_type, const char *bearer,
                     JsonDocument &filter, JsonDocument &out) {
  if (xSemaphoreTake(tls_gate(), pdMS_TO_TICKS(30000)) != pdTRUE) return -2;
  if (!tls_memory_ok()) {
    Serial.printf("Tiger : memoire interne insuffisante pour TLS (bloc max %u)\n",
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    xSemaphoreGive(tls_gate());
    return -3;
  }
  int code = http_json_locked(url, post, body, content_type, bearer, filter, out);
  xSemaphoreGive(tls_gate());
  return code;
}

static int http_json_locked(const String &url, bool post, const String &body, const char *content_type, const char *bearer,
                            JsonDocument &filter, JsonDocument &out) {
  WiFiClientSecure client;
  client.setHandshakeTimeout(15);  // secondes (120 par défaut)
  client.setInsecure();  // réseau de confiance ; pas de magasin de certificats embarqué
  HTTPClient http;
  http.useHTTP10(true);  // évite le codage « chunked », que le flux JSON ne gère pas
  http.setTimeout(20000);
  if (!http.begin(client, url)) return -1;
  if (bearer) http.addHeader("Authorization", String("Bearer ") + bearer);
  int code;
  if (post) {
    http.addHeader("Content-Type", content_type);
    code = http.POST(body);
  } else {
    code = http.GET();
  }
  if (code <= 0) {
    // Diagnostic : distingue DNS, connexion TCP et poignée de main TLS
    char tls[96] = "";
    int tls_err = client.lastError(tls, sizeof(tls));
    String host = url.substring(url.indexOf("//") + 2);
    host = host.substring(0, host.indexOf('/'));
    IPAddress ip;
    int dns = WiFi.hostByName(host.c_str(), ip);
    Serial.printf("Tiger : echec HTTP %d (%s) | WiFi %d | DNS %s -> %s | TLS %d %s | interne libre %u, bloc max %u\n", code,
                  http.errorToString(code).c_str(), (int)WiFi.status(), host.c_str(), dns == 1 ? ip.toString().c_str() : "ECHEC", tls_err, tls,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  }
  if (code > 0) {
    // Corps lu en entier, en PSRAM, avant l'analyse : la lecture directe du flux était tronquée (IncompleteInput)
    uint32_t t0 = millis();
    int len = http.getSize();
    size_t cap = len > 0 ? (size_t)len + 1 : 65536;
    char *buf = (char *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t n = 0;
    if (buf) {
      WiFiClient *st = http.getStreamPtr();
      uint32_t last = millis();
      while (http.connected() && !(len > 0 && n >= (size_t)len)) {
        int av = st->available();
        if (av > 0) {
          if (n + av + 1 > cap) {
            size_t ncap = cap * 2 + av;
            char *nb = (char *)heap_caps_realloc(buf, ncap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!nb) break;
            buf = nb;
            cap = ncap;
          }
          int r = st->readBytes(buf + n, av);
          if (r > 0) n += r;
          last = millis();
        } else {
          if (millis() - last > 15000) break;
          vTaskDelay(pdMS_TO_TICKS(5));
        }
      }
      buf[n] = 0;
      DeserializationError e = deserializeJson(out, buf, n, DeserializationOption::Filter(filter));
      if (e) Serial.printf("Tiger : JSON illisible (%s) pour %s\n", e.c_str(), url.substring(0, 60).c_str());
      heap_caps_free(buf);
    } else {
      Serial.println("Tiger : tampon PSRAM non alloue");
    }
    Serial.printf("Tiger : %s -> HTTP %d, %u octets, %u ms | interne libre %u\n", url.substring(8, 40).c_str(), code, (unsigned)n,
                  (unsigned)(millis() - t0), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  }
  http.end();
  return code;
}

static String url_encode(const String &s) {
  String r;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') r += c;
    else {
      r += '%';
      r += hex[(c >> 4) & 15];
      r += hex[c & 15];
    }
  }
  return r;
}

static String friendly_error(const String &code, int http) {
  if (code == "EMAIL_NOT_FOUND" || code == "INVALID_PASSWORD" || code == "INVALID_LOGIN_CREDENTIALS")
    return "E-mail ou mot de passe incorrect";
  if (code == "INVALID_EMAIL") return "Adresse e-mail invalide";
  if (code == "USER_DISABLED") return "Compte désactivé";
  if (code.startsWith("TOO_MANY_ATTEMPTS")) return "Trop d'essais : réessayez plus tard";
  if (code.length()) return "Refusé par Tiger Studio : " + code;
  return "Erreur réseau (code " + String(http) + ")";
}

// ---------------------------------------------------------------------
static void save_account() {
  Preferences pr;
  pr.begin("tiger", false);
  pr.putString("email", g_email);
  pr.putString("rtoken", g_refresh);
  pr.putString("uid", g_uid);
  pr.end();
}

static void clear_account() {
  Preferences pr;
  pr.begin("tiger", false);
  pr.clear();
  pr.end();
  g_email = g_refresh = g_uid = g_id_token = "";
  g_token_expiry = 0;
}

static bool do_login(const String &email, const String &pass) {
  set_state(TigerState::SigningIn);
  JsonDocument filter;
  filter["idToken"] = true;
  filter["refreshToken"] = true;
  filter["localId"] = true;
  filter["expiresIn"] = true;
  filter["error"]["message"] = true;
  JsonDocument body;
  body["email"] = email;
  body["password"] = pass;
  body["returnSecureToken"] = true;
  String b;
  serializeJson(body, b);
  JsonDocument out(&g_psram);
  String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + FB_KEY;
  int code = http_json(url, true, b, "application/json", nullptr, filter, out);
  b = "";
  if (code == 200 && out["idToken"].is<const char *>()) {
    g_email = email;
    g_id_token = out["idToken"].as<const char *>();
    g_refresh = out["refreshToken"].as<const char *>();
    g_uid = out["localId"].as<const char *>();
    int ttl = atoi(out["expiresIn"] | "3600");
    g_token_expiry = millis() + (uint32_t)ttl * 1000UL;
    save_account();
    Serial.println("Tiger : connecte");
    return true;
  }
  String err = out["error"]["message"] | "";
  Serial.printf("Tiger : connexion refusee (HTTP %d, %s)\n", code, err.c_str());
  set_state(TigerState::SignedOut, friendly_error(err, code));
  return false;
}

static bool ensure_token() {
  if (g_id_token.length() && (int32_t)(g_token_expiry - millis()) > 60000) return true;
  if (g_refresh.isEmpty()) return false;
  JsonDocument filter;
  filter["id_token"] = true;
  filter["refresh_token"] = true;
  filter["user_id"] = true;
  filter["expires_in"] = true;
  filter["error"]["message"] = true;
  JsonDocument out(&g_psram);
  String url = String("https://securetoken.googleapis.com/v1/token?key=") + FB_KEY;
  String body = "grant_type=refresh_token&refresh_token=" + url_encode(g_refresh);
  int code = http_json(url, true, body, "application/x-www-form-urlencoded", nullptr, filter, out);
  if (code == 200 && out["id_token"].is<const char *>()) {
    g_id_token = out["id_token"].as<const char *>();
    if (out["refresh_token"].is<const char *>()) g_refresh = out["refresh_token"].as<const char *>();
    if (out["user_id"].is<const char *>()) g_uid = out["user_id"].as<const char *>();
    g_token_expiry = millis() + (uint32_t)atoi(out["expires_in"] | "3600") * 1000UL;
    save_account();
    return true;
  }
  String err = out["error"]["message"] | "";
  Serial.printf("Tiger : renouvellement refuse (HTTP %d, %s)\n", code, err.c_str());
  if (err == "TOKEN_EXPIRED" || err == "USER_DISABLED" || err == "USER_NOT_FOUND" || err == "INVALID_REFRESH_TOKEN") {
    clear_account();
    set_state(TigerState::SignedOut, "Session expirée : reconnectez-vous");
  } else {
    set_state(TigerState::Error, friendly_error(err, code));
  }
  return false;
}

// ---------------------------------------------------------------------
static void load_table(const char *url, std::map<int, String> &dst) {
  JsonDocument filter;
  filter[0]["id"] = true;
  filter[0]["name"] = true;
  filter[0]["label"] = true;
  JsonDocument out(&g_psram);
  int code = http_json(url, false, "", nullptr, nullptr, filter, out);
  if (code != 200) {
    Serial.printf("Tiger : table de reference indisponible (HTTP %d)\n", code);
    return;
  }
  std::map<int, String> tmp;
  for (JsonObject o : out.as<JsonArray>()) {
    int id = o["id"] | -1;
    String name = o["name"] | (o["label"] | "");
    if (id >= 0 && name.length()) tmp[id] = name;
  }
  if (!tmp.empty()) {
    Lock lk;
    dst = tmp;
  }
  Serial.printf("Tiger : %u noms charges\n", (unsigned)tmp.size());
}

static double numv(JsonVariantConst fv) {
  if (fv["integerValue"].is<const char *>()) return atof(fv["integerValue"].as<const char *>());
  if (fv["integerValue"].is<long>()) return fv["integerValue"].as<long>();
  if (fv["doubleValue"].is<double>()) return fv["doubleValue"].as<double>();
  return -1;
}

static String strv(JsonVariantConst fv) { return String((const char *)(fv["stringValue"] | "")); }

static bool do_sync() {
  if (!ensure_token()) return false;
  set_state(TigerState::Syncing);
  if (!g_tables_loaded) {
    load_table(URL_BRANDS, g_brands);
    load_table(URL_MATERIALS, g_materials);
    g_tables_loaded = true;
  }
  std::vector<Spool> list;
  String page_token;
  for (int page = 0; page < 10; page++) {
    JsonDocument filter;
    filter["nextPageToken"] = true;
    filter["error"]["message"] = true;
    for (const char *k : {"uid", "tag_uid", "twin_uid", "id_brand", "id_material", "color_name", "online_color_list",
                          "weight_available", "capacity", "deleted"})
      filter["documents"][0]["fields"][k] = true;
    String url = String("https://firestore.googleapis.com/v1/projects/") + FB_PROJECT + "/databases/(default)/documents/users/" +
                 g_uid + "/inventory?pageSize=100";
    for (const char *k : {"uid", "tag_uid", "twin_uid", "id_brand", "id_material", "color_name", "online_color_list",
                          "weight_available", "capacity", "deleted"})
      url += String("&mask.fieldPaths=") + k;
    if (page_token.length()) url += "&pageToken=" + url_encode(page_token);
    JsonDocument out(&g_psram);
    int code = http_json(url, false, "", nullptr, g_id_token.c_str(), filter, out);
    if (code != 200) {
      String err = out["error"]["message"] | "";
      Serial.printf("Tiger : inventaire refuse (HTTP %d, %s)\n", code, err.c_str());
      set_state(TigerState::Error, code == 403 ? "Accès à l'inventaire refusé" : friendly_error(err, code));
      return false;
    }
    for (JsonObject d : out["documents"].as<JsonArray>()) {
      JsonObject f = d["fields"];
      if (f["deleted"]["booleanValue"] | false) continue;
      Spool s;
      s.uid = strv(f["uid"]);
      s.tag = strv(f["tag_uid"]);
      s.twin = strv(f["twin_uid"]);
      s.brand = (int)numv(f["id_brand"]);
      s.material = (int)numv(f["id_material"]);
      s.color_name = strv(f["color_name"]);
      s.weight = (int)numv(f["weight_available"]);
      s.capacity = (int)numv(f["capacity"]);
      JsonArray colors = f["online_color_list"]["arrayValue"]["values"];
      if (!colors.isNull() && colors.size() > 0) {
        String hex = strv(colors[0]);
        if (hex.startsWith("#")) hex.remove(0, 1);
        if (hex.length() >= 6) s.color = strtoul(hex.substring(0, 6).c_str(), nullptr, 16);
      }
      if (list.size() < MAX_SPOOLS) list.push_back(s);
    }
    page_token = out["nextPageToken"] | "";
    if (page_token.isEmpty()) break;
  }
  {
    Lock lk;
    g_spools.swap(list);
  }
  g_last_sync = millis();
  Serial.printf("Tiger : %u bobines synchronisees\n", (unsigned)g_spools.size());
  set_state(TigerState::Ready);
  return true;
}

// Suspend les connexions MQTT des imprimantes seulement si la mémoire interne ne permet pas une session TLS de plus
struct PrinterPause {
  bool active = false;
  PrinterPause() {
    size_t blk = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (blk < 140000) {
      Serial.printf("Tiger : pause des imprimantes (bloc interne max %u)\n", (unsigned)blk);
      bambu_pause(true);
      active = true;
    }
  }
  ~PrinterPause() {
    if (active) {
      bambu_pause(false);
      Serial.println("Tiger : reprise des imprimantes");
    }
  }
};

static void tiger_task(void *) {
  for (;;) {
    if (g_req_logout) {
      g_req_logout = false;
      clear_account();
      {
        Lock lk;
        g_spools.clear();
      }
      set_state(TigerState::SignedOut);
    }
    bool online = wifi_state() == WifiState::Connected;
    if (g_req_login && online) {
      g_req_login = false;
      String e = g_req_email, p = g_req_pass;
      g_req_pass = "";
      bool ok;
      {
        PrinterPause pp;  // libère la mémoire interne des sessions TLS des imprimantes si elle manque
        ok = do_login(e, p);
      }
      if (ok) g_req_sync = true;
    } else if (g_req_login && !online) {
      g_req_login = false;
      g_req_pass = "";
      set_state(TigerState::SignedOut, "WiFi non connecté");
    }
    if (online && g_refresh.length() && g_state != TigerState::SigningIn) {
      uint32_t now = millis();
      bool due = g_last_sync == 0 ? now >= g_next_try : (now - g_last_sync > SYNC_PERIOD_MS);
      if (g_req_sync || (due && now >= g_next_try)) {
        g_req_sync = false;
        bool ok;
        {
          PrinterPause pp;
          ok = do_sync();
        }
        if (!ok) g_next_try = millis() + RETRY_PERIOD_MS;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// ---------------------------------------------------------------------
static bool g_task_started = false;
static void ensure_task() {
  if (g_task_started) return;
  g_task_started = true;
  xTaskCreatePinnedToCore(tiger_task, "tiger_net", 16384, nullptr, 0, nullptr, 0);
}

void tiger_begin() {
  g_mutex = xSemaphoreCreateMutex();
  Preferences pr;
  pr.begin("tiger", false);
  g_email = pr.getString("email", "");
  g_refresh = pr.getString("rtoken", "");
  g_uid = pr.getString("uid", "");
  pr.end();
  g_next_try = 30000;  // première synchro 30 s après le démarrage : les imprimantes se connectent d'abord
  if (g_refresh.length()) {
    g_state = TigerState::Syncing;
    ensure_task();  // sans compte, la tâche (et sa pile de 24 Ko en mémoire interne) n'est pas créée
  }
}

bool tiger_signed_in() { return g_refresh.length() > 0; }
String tiger_email() { return g_email; }

void tiger_login(const String &email, const String &password) {
  ensure_task();
  g_req_email = email;
  g_req_pass = password;
  g_req_login = true;
  set_state(TigerState::SigningIn);
}

void tiger_logout() { g_req_logout = true; }
void tiger_sync_now() {
  g_req_sync = true;
  g_next_try = 0;
}
TigerState tiger_state() { return g_state; }
String tiger_message() {
  Lock lk;
  return g_message;
}
int tiger_revision() { return g_rev; }
int tiger_count() {
  Lock lk;
  return (int)g_spools.size();
}
bool tiger_get(int i, Spool &out) {
  Lock lk;
  if (i < 0 || i >= (int)g_spools.size()) return false;
  out = g_spools[i];
  return true;
}
String tiger_brand(int id) {
  Lock lk;
  auto it = g_brands.find(id);
  return it == g_brands.end() ? String("Marque ") + id : it->second;
}
String tiger_material(int id) {
  Lock lk;
  auto it = g_materials.find(id);
  return it == g_materials.end() ? String("Matière ") + id : it->second;
}

static bool same_tag(const String &a, const String &b) {
  if (a.length() < 4 || b.length() < 4) return false;
  return a.equalsIgnoreCase(b);
}

int tiger_find_tag(const String &tag_uid) {
  if (tag_uid.length() < 4) return -1;
  Lock lk;
  for (size_t i = 0; i < g_spools.size(); i++) {
    const Spool &s = g_spools[i];
    if (same_tag(s.uid, tag_uid) || same_tag(s.tag, tag_uid) || same_tag(s.twin, tag_uid)) return (int)i;
  }
  return -1;
}

int tiger_find_color(uint32_t color_rgb, const String &material) {
  Lock lk;
  for (size_t i = 0; i < g_spools.size(); i++) {
    const Spool &s = g_spools[i];
    if (s.color != color_rgb) continue;
    auto it = g_materials.find(s.material);
    if (it != g_materials.end() && it->second.equalsIgnoreCase(material)) return (int)i;
  }
  return -1;
}
