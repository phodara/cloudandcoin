// ---------------- Display flush ----------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

// ---------------- Touch read ----------------
bool readMappedTouch(int &x, int &y, int *pressure = nullptr) {
  if (!ts.touched()) {
    if (pressure) *pressure = 0;
    return false;
  }

  TS_Point p = ts.getPoint();
  if (pressure) *pressure = p.z;
  if (p.z < touchPressureMin) return false;

  x = map(p.x, touchMinX, touchMaxX, screenWidth - 1, 0);
  y = map(p.y, touchMinY, touchMaxY, screenHeight - 1, 0);
  x = constrain(x, 0, screenWidth - 1);
  y = constrain(y, 0, screenHeight - 1);
  return true;
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  (void)indev_driver;

  int x = 0;
  int y = 0;
  if (readMappedTouch(x, y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void logTouchDebug() {
#if TOUCH_DEBUG
  int x = 0;
  int y = 0;
  int pressure = 0;
  if (!readMappedTouch(x, y, &pressure)) {
    if (touchDebugWasDown) {
      Serial.println("Touch debug: released");
      touchDebugWasDown = false;
    }
    lastTouchDebugMs = 0;
    return;
  }

  unsigned long now = millis();
  if (!touchDebugWasDown || now - lastTouchDebugMs >= 250) {
    Serial.printf(
      "Touch debug: %s pressure=%d mapped(%d,%d)\n",
      touchDebugWasDown ? "held" : "pressed",
      pressure,
      x,
      y
    );
    lastTouchDebugMs = now;
  }
  touchDebugWasDown = true;
#endif
}
