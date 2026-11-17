#pragma once
#include <Arduino.h>

// Inventaire des bobines Tiger Studio (base Firebase de TigerTag, lue avec le compte de l'utilisateur).

struct Spool {
  String uid;         // identifiant de puce RFID (champ « uid »)
  String tag;         // champ « tag_uid »
  String twin;        // champ « twin_tag_uid »
  int brand = 0;      // identifiant de marque (voir tiger_brand)
  int material = 0;   // identifiant de matière (voir tiger_material)
  String color_name;
  uint32_t color = 0; // RRGGBB
  int weight = -1;    // grammes disponibles, -1 si inconnu
  int capacity = -1;  // grammes, capacité de la bobine
};

enum class TigerState { SignedOut, SigningIn, Syncing, Ready, Error };

void tiger_begin();  // lit le compte enregistré et lance la tâche réseau
bool tiger_signed_in();
String tiger_email();
void tiger_login(const String &email, const String &password);  // le mot de passe n'est pas conservé
void tiger_logout();
void tiger_sync_now();
TigerState tiger_state();
String tiger_message();  // dernier message d'erreur, lisible
int tiger_revision();    // augmente à chaque synchronisation réussie ou changement d'état
int tiger_count();
bool tiger_get(int i, Spool &out);
String tiger_brand(int id);
String tiger_material(int id);

// Rapprochement avec une bobine de l'AMS.
int tiger_find_tag(const String &tag_uid);                        // identifiant de puce identique, ou -1
int tiger_find_color(uint32_t color_rgb, const String &material);  // même couleur et même matière (approximatif), ou -1
