#pragma once

#include <esphome/core/defines.h>
#ifdef USE_LIGHT

#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "hap_entity.h"

namespace esphome {
namespace homekit {

class LightEntity : public HAPEntity {
 private:
  static constexpr const char *TAG = "LightEntity";
  light::LightState *lightPtr;

  /* ================= WRITE CALLBACK ================= */

  static int light_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv) {
    auto *lightPtr = (light::LightState *) serv_priv;
    int ret = HAP_SUCCESS;

    for (int i = 0; i < count; i++) {
      auto *write = &write_data[i];

      if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
        write->val.b ? lightPtr->turn_on().set_save(true).perform()
                     : lightPtr->turn_off().set_save(true).perform();
        hap_char_update_val(write->hc, &(write->val));
        *(write->status) = HAP_STATUS_SUCCESS;
      }

      else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_BRIGHTNESS)) {
        lightPtr->make_call().set_brightness(write->val.i / 100.0f).set_save(true).perform();
        hap_char_update_val(write->hc, &(write->val));
        *(write->status) = HAP_STATUS_SUCCESS;
      }

      else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_COLOR_TEMPERATURE)) {
        lightPtr->make_call().set_color_temperature(write->val.u).set_save(true).perform();
        hap_char_update_val(write->hc, &(write->val));
        *(write->status) = HAP_STATUS_SUCCESS;
      }

      else {
        *(write->status) = HAP_STATUS_RES_ABSENT;
      }
    }
    return ret;
  }

  /* ================= STATE UPDATE ================= */

  static void on_light_update(light::LightState *obj) {
    hap_acc_t *acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
    if (!acc) return;

    hap_serv_t *hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_LIGHTBULB);
    if (!hs) return;

    hap_char_t *on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
    hap_val_t state;
    state.b = obj->current_values.get_state();
    hap_char_update_val(on_char, &state);

    if (obj->get_traits().supports_color_capability(light::ColorCapability::BRIGHTNESS)) {
      hap_char_t *level_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_BRIGHTNESS);
      hap_val_t level;
      level.i = int(obj->current_values.get_brightness() * 100);
      hap_char_update_val(level_char, &level);
    }
  }

  /* ================= IDENTIFY ================= */

  static int acc_identify(hap_acc_t *) {
    ESP_LOGI(TAG, "Accessory identified");
    return HAP_SUCCESS;
  }

  /* ================= LISTENER ================= */

  class HomeKitLightListener : public light::LightTargetStateReachedListener {
   public:
    HomeKitLightListener(LightEntity *entity, light::LightState *light) : entity_(entity), light_(light) {}

    void on_light_target_state_reached() override {
      entity_->on_light_update(light_);
    }

   private:
    LightEntity *entity_;
    light::LightState *light_;
  };

 public:
  LightEntity(light::LightState *lightPtr) : HAPEntity({{MODEL, "HAP-LIGHT"}}), lightPtr(lightPtr) {}

  void setup() {
    hap_acc_cfg_t acc_cfg = {
        .model = strdup(accessory_info[MODEL]),
        .manufacturer = strdup(accessory_info[MANUFACTURER]),
        .fw_rev = strdup(accessory_info[FW_REV]),
        .hw_rev = nullptr,
        .pv = strdup("1.1.0"),
        .cid = HAP_CID_BRIDGE,
        .identify_routine = acc_identify,
    };

    std::string name = lightPtr->get_name();
    acc_cfg.name = strdup(name.c_str());
    acc_cfg.serial_num = strdup(std::to_string(lightPtr->get_object_id_hash()).c_str());

    hap_acc_t *accessory = hap_acc_create(&acc_cfg);
    hap_serv_t *service = hap_serv_lightbulb_create(lightPtr->current_values.get_state());

    hap_serv_add_char(service, hap_char_name_create(strdup(name.c_str())));

    if (lightPtr->get_traits().supports_color_capability(light::ColorCapability::BRIGHTNESS))
      hap_serv_add_char(service, hap_char_brightness_create(lightPtr->current_values.get_brightness() * 100));

    hap_serv_set_priv(service, lightPtr);
    hap_serv_set_write_cb(service, light_write);
    hap_acc_add_serv(accessory, service);

    hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(lightPtr->get_object_id_hash()).c_str()));

    if (!lightPtr->is_internal()) {
      auto *listener = new HomeKitLightListener(this, lightPtr);
      lightPtr->add_target_state_reached_listener(listener);
    }

    ESP_LOGI(TAG, "Light '%s' linked to HomeKit", name.c_str());
  }
};

}  // namespace homekit
}  // namespace esphome

#endif
