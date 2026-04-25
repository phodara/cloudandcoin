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
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  (void)indev_driver;

  if (!ts.touched()) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  TS_Point p = ts.getPoint();

  int x = map(p.x, touchMinX, touchMaxX, screenWidth - 1, 0);
  int y = map(p.y, touchMinY, touchMaxY, screenHeight - 1, 0);

  data->state = LV_INDEV_STATE_PR;
  data->point.x = constrain(x, 0, screenWidth - 1);
  data->point.y = constrain(y, 0, screenHeight - 1);
}

void logTouchDebug() {
#if TOUCH_DEBUG
  if (!ts.touched()) {
    if (touchDebugWasDown) {
      Serial.println("Touch debug: released");
      touchDebugWasDown = false;
    }
    return;
  }

  TS_Point p = ts.getPoint();
  int x = map(p.x, touchMinX, touchMaxX, screenWidth - 1, 0);
  int y = map(p.y, touchMinY, touchMaxY, screenHeight - 1, 0);

  x = constrain(x, 0, screenWidth - 1);
  y = constrain(y, 0, screenHeight - 1);

  Serial.printf("Touch debug: raw(%d,%d,%d) mapped(%d,%d)\n", p.x, p.y, p.z, x, y);
  touchDebugWasDown = true;
#endif
}

