// ---------------- Styling helpers ----------------
void style_screen(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x111418), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void style_panel(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x1c1f24), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 12, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// ---------------- UI build ----------------
void buildUI() {
  lv_obj_t *scr = lv_scr_act();
  style_screen(scr);

  lv_obj_t *title = lv_label_create(scr);
  char titleText[24];
  snprintf(titleText, sizeof(titleText), "Cloud and Coin %s", APP_VERSION);
  lv_label_set_text(title, titleText);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(title, 14, 12);

  status_label = lv_label_create(scr);
  lv_label_set_text(status_label, "Starting...");
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_width(status_label, 160);
  lv_label_set_long_mode(status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(status_label, 306, 14);

  battery_label = lv_label_create(scr);
  lv_label_set_text(battery_label, batteryMonitorEnabled() ? "Batt ..." : "Batt off");
  lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_width(battery_label, 180);
  lv_label_set_long_mode(battery_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(battery_label, 14, 298);

  memory_label = lv_label_create(scr);
  lv_label_set_text(memory_label, "Mem ...");
  lv_obj_set_style_text_font(memory_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(memory_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_width(memory_label, 260);
  lv_label_set_long_mode(memory_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(memory_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(memory_label, 204, 298);

  setup_page = lv_obj_create(scr);
  lv_obj_set_size(setup_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(setup_page, PAGE_X, PAGE_Y);
  style_panel(setup_page);
  lv_obj_add_flag(setup_page, LV_OBJ_FLAG_HIDDEN);

  setup_message_label = lv_label_create(setup_page);
  lv_label_set_text(setup_message_label, "");
  lv_obj_set_width(setup_message_label, PAGE_W - 24);
  lv_label_set_long_mode(setup_message_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(setup_message_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(setup_message_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(setup_message_label, 0, 14);

  // Weather page
  weather_page = lv_obj_create(scr);
  lv_obj_set_size(weather_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(weather_page, PAGE_X, PAGE_Y);
  style_panel(weather_page);

  weather_title_label = lv_label_create(scr);
  lv_label_set_text(weather_title_label, "Weather");
  lv_obj_set_style_text_font(weather_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(weather_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);

  weather_temp_label = lv_label_create(weather_page);
  lv_label_set_text(weather_temp_label, "-- F");
  lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(weather_temp_label, 0, 26);

  weather_cond_label = lv_label_create(weather_page);
  lv_label_set_text(weather_cond_label, "Loading...");
  lv_obj_set_style_text_font(weather_cond_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_cond_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(weather_cond_label, 0, 58);

  weather_hi_label = lv_label_create(weather_page);
  lv_label_set_text(weather_hi_label, "Hi: -- F");
  lv_obj_set_style_text_font(weather_hi_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_hi_label, lv_color_hex(0xF4B400), 0);
  lv_obj_set_pos(weather_hi_label, 0, 88);

  weather_lo_label = lv_label_create(weather_page);
  lv_label_set_text(weather_lo_label, "Lo: -- F");
  lv_obj_set_style_text_font(weather_lo_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_lo_label, lv_color_hex(0x7EC8FF), 0);
  lv_obj_set_pos(weather_lo_label, 110, 88);

  weather_pressure_label = lv_label_create(weather_page);
  lv_label_set_text(weather_pressure_label, "Pressure: --");
  lv_obj_set_style_text_font(weather_pressure_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_pressure_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_pos(weather_pressure_label, 248, 88);

  for (int i = 0; i < 4; i++) {
    int x = 10 + (i * 108);
    int y = 105;

    forecast_box[i] = lv_obj_create(weather_page);
    lv_obj_remove_style_all(forecast_box[i]);
    lv_obj_set_size(forecast_box[i], 98, 92);
    lv_obj_set_pos(forecast_box[i], x, y);
    lv_obj_clear_flag(forecast_box[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(forecast_box[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(forecast_box[i], 1, 0);
    lv_obj_set_style_border_color(forecast_box[i], lv_color_hex(0x666666), 0);

    forecast_day_label[i] = lv_label_create(forecast_box[i]);
    lv_label_set_text(forecast_day_label[i], "---");
    lv_obj_set_style_text_font(forecast_day_label[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_day_label[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(forecast_day_label[i], 8, 6);

    forecast_temp_label[i] = lv_label_create(forecast_box[i]);
    lv_label_set_text(forecast_temp_label[i], "--/--");
    lv_obj_set_style_text_font(forecast_temp_label[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_temp_label[i], lv_color_hex(0xF4B400), 0);
    lv_obj_set_pos(forecast_temp_label[i], 8, 32);

    forecast_cond_label[i] = lv_label_create(forecast_box[i]);
    lv_label_set_text(forecast_cond_label[i], "N/A");
    lv_obj_set_style_text_font(forecast_cond_label[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_cond_label[i], lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_pos(forecast_cond_label[i], 8, 58);
  }

  // Crypto page
  crypto_page = lv_obj_create(scr);
  lv_obj_set_size(crypto_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(crypto_page, PAGE_X, PAGE_Y);
  style_panel(crypto_page);

  crypto_title_label = lv_label_create(scr);
  lv_label_set_text(crypto_title_label, "Crypto");
  lv_obj_set_style_text_font(crypto_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(crypto_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(crypto_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < CRYPTO_VISIBLE_ROWS; i++) {
    crypto_value_labels[i] = lv_label_create(crypto_page);
    lv_label_set_text(crypto_value_labels[i], "--");
    lv_obj_set_style_text_font(crypto_value_labels[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(crypto_value_labels[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(crypto_value_labels[i], CRYPTO_LABEL_X, CRYPTO_ROW_LABEL_Y[i]);

    crypto_boxes[i] = lv_obj_create(crypto_page);
    lv_obj_remove_style_all(crypto_boxes[i]);
    lv_obj_set_size(crypto_boxes[i], BOX_W, BOX_H);
    lv_obj_set_pos(crypto_boxes[i], CRYPTO_BOX_X, CRYPTO_ROW_BOX_Y[i]);
    lv_obj_clear_flag(crypto_boxes[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(crypto_boxes[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(crypto_boxes[i], 1, 0);
    lv_obj_set_style_border_color(crypto_boxes[i], lv_color_hex(0x666666), 0);
  }

  // Stocks page
  stock_page = lv_obj_create(scr);
  lv_obj_set_size(stock_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(stock_page, PAGE_X, PAGE_Y);
  style_panel(stock_page);

  stock_title_label = lv_label_create(scr);
  lv_label_set_text(stock_title_label, "Stocks");
  lv_obj_set_style_text_font(stock_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(stock_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(stock_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(stock_title_label, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < STOCK_VISIBLE_ROWS; i++) {
    stock_value_labels[i] = lv_label_create(stock_page);
    lv_label_set_text(stock_value_labels[i], "--");
    lv_obj_set_width(stock_value_labels[i], PAGE_W - 24);
    lv_label_set_long_mode(stock_value_labels[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(stock_value_labels[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(stock_value_labels[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(stock_value_labels[i], 0, 24 + (i * 48));
  }

  // Pair trading page
  pair_page = lv_obj_create(scr);
  lv_obj_set_size(pair_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(pair_page, PAGE_X, PAGE_Y);
  style_panel(pair_page);

  pair_title_label = lv_label_create(scr);
  lv_label_set_text(pair_title_label, "Pair Trading");
  lv_obj_set_style_text_font(pair_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(pair_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(pair_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(pair_title_label, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < PAIR_VISIBLE_ROWS; i++) {
    pair_value_labels[i] = lv_label_create(pair_page);
    lv_label_set_text(pair_value_labels[i], "--");
    lv_obj_set_width(pair_value_labels[i], PAGE_W - 24);
    lv_label_set_long_mode(pair_value_labels[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(pair_value_labels[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pair_value_labels[i], lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_pos(pair_value_labels[i], 0, 24 + (i * 48));
  }

#if ENABLE_TRADING_SIGNALS
  // Trading signals page
  signal_page = lv_obj_create(scr);
  lv_obj_set_size(signal_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(signal_page, PAGE_X, PAGE_Y);
  style_panel(signal_page);

  signal_title_label = lv_label_create(scr);
  lv_label_set_text(signal_title_label, "Signals");
  lv_obj_set_style_text_font(signal_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(signal_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(signal_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(signal_title_label, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < SIGNAL_VISIBLE_ROWS; i++) {
    signal_value_labels[i] = lv_label_create(signal_page);
    lv_label_set_text(signal_value_labels[i], "--");
    lv_obj_set_width(signal_value_labels[i], PAGE_W - 24);
    lv_label_set_long_mode(signal_value_labels[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(signal_value_labels[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(signal_value_labels[i], lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_pos(signal_value_labels[i], 0, 24 + (i * 48));
  }
#endif

  // System page
  battery_page = lv_obj_create(scr);
  lv_obj_set_size(battery_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(battery_page, PAGE_X, PAGE_Y);
  style_panel(battery_page);

  battery_title_label = lv_label_create(scr);
  lv_label_set_text(battery_title_label, "System");
  lv_obj_set_style_text_font(battery_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(battery_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(battery_title_label, LV_OBJ_FLAG_HIDDEN);

  battery_percent_label = lv_label_create(battery_page);
  lv_label_set_text(battery_percent_label, "--%");
  lv_obj_set_style_text_font(battery_percent_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_percent_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(battery_percent_label, 0, 24);

  battery_state_label = lv_label_create(battery_page);
  lv_label_set_text(battery_state_label, batteryMonitorEnabled() ? "Reading battery..." : "Monitor disabled");
  lv_obj_set_width(battery_state_label, PAGE_W - 24);
  lv_label_set_long_mode(battery_state_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(battery_state_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_state_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(battery_state_label, 82, 24);

  battery_level_bar = lv_obj_create(battery_page);
  lv_obj_remove_style_all(battery_level_bar);
  lv_obj_set_size(battery_level_bar, PAGE_W - 24, 18);
  lv_obj_set_pos(battery_level_bar, 0, 58);
  lv_obj_clear_flag(battery_level_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(battery_level_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(battery_level_bar, lv_color_hex(0x333840), 0);
  lv_obj_set_style_border_width(battery_level_bar, 0, 0);

  battery_level_fill = lv_obj_create(battery_level_bar);
  lv_obj_remove_style_all(battery_level_fill);
  lv_obj_set_size(battery_level_fill, 0, 18);
  lv_obj_set_pos(battery_level_fill, 0, 0);
  lv_obj_clear_flag(battery_level_fill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(battery_level_fill, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(battery_level_fill, lv_color_hex(0x7ED957), 0);
  lv_obj_set_style_border_width(battery_level_fill, 0, 0);

  battery_voltage_label = lv_label_create(battery_page);
  lv_label_set_text(battery_voltage_label, "Voltage: --");
  lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_voltage_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(battery_voltage_label, 0, 88);

  battery_adc_label = lv_label_create(battery_page);
  lv_label_set_text(battery_adc_label, "ADC: -- mV");
  lv_obj_set_style_text_font(battery_adc_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_adc_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(battery_adc_label, 0, 116);

  battery_raw_label = lv_label_create(battery_page);
  lv_label_set_text(battery_raw_label, "Raw: --");
  lv_obj_set_style_text_font(battery_raw_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_raw_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(battery_raw_label, 0, 144);

  battery_range_label = lv_label_create(battery_page);
  lv_label_set_text(battery_range_label, "Range: --");
  lv_obj_set_width(battery_range_label, 212);
  lv_label_set_long_mode(battery_range_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(battery_range_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_range_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_pos(battery_range_label, 0, 172);

  system_wifi_label = lv_label_create(battery_page);
  lv_label_set_text(system_wifi_label, "WiFi: --");
  lv_obj_set_width(system_wifi_label, 210);
  lv_label_set_long_mode(system_wifi_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(system_wifi_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(system_wifi_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(system_wifi_label, 230, 88);

  system_ip_label = lv_label_create(battery_page);
  lv_label_set_text(system_ip_label, "IP: --");
  lv_obj_set_width(system_ip_label, 210);
  lv_label_set_long_mode(system_ip_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(system_ip_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(system_ip_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(system_ip_label, 230, 116);

  system_signal_label = lv_label_create(battery_page);
  lv_label_set_text(system_signal_label, "Signal: --");
  lv_obj_set_width(system_signal_label, 210);
  lv_label_set_long_mode(system_signal_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(system_signal_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(system_signal_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(system_signal_label, 230, 144);

  system_sd_label = lv_label_create(battery_page);
  lv_label_set_text(system_sd_label, "SD: --");
  lv_obj_set_width(system_sd_label, 210);
  lv_label_set_long_mode(system_sd_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(system_sd_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(system_sd_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(system_sd_label, 230, 172);

  system_memory_label = lv_label_create(battery_page);
  lv_label_set_text(system_memory_label, "Mem: --");
  lv_obj_set_width(system_memory_label, 210);
  lv_label_set_long_mode(system_memory_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(system_memory_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(system_memory_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(system_memory_label, 0, 200);

  system_version_label = lv_label_create(battery_page);
  lv_label_set_text(system_version_label, "FW: --");
  lv_obj_set_width(system_version_label, 210);
  lv_label_set_long_mode(system_version_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(system_version_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(system_version_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(system_version_label, 230, 200);
  updateBatteryPage();

  showPage(0);
}
