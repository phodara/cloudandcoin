// ---------------- World news ----------------
String extractXmlTag(const String &block, const char *tag) {
  String openTag = "<";
  openTag += tag;
  int start = block.indexOf(openTag);
  if (start < 0) return "";
  start = block.indexOf('>', start);
  if (start < 0) return "";
  start++;

  String closeTag = "</";
  closeTag += tag;
  closeTag += ">";
  int end = block.indexOf(closeTag, start);
  if (end < 0) return "";

  return block.substring(start, end);
}

void sanitizeNewsText(String &text) {
  text.replace("<![CDATA[", "");
  text.replace("]]>", "");

  int tagStart = text.indexOf('<');
  while (tagStart >= 0) {
    int tagEnd = text.indexOf('>', tagStart);
    if (tagEnd < 0) break;
    text.remove(tagStart, tagEnd - tagStart + 1);
    tagStart = text.indexOf('<');
  }

  text.replace("&amp;", "&");
  text.replace("&quot;", "\"");
  text.replace("&apos;", "'");
  text.replace("&#39;", "'");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.replace("\r", " ");
  text.replace("\n", " ");

  while (text.indexOf("  ") >= 0) text.replace("  ", " ");
  text.trim();
}

void copyNewsText(char *dest, size_t destSize, String text) {
  sanitizeNewsText(text);
  if (text.length() >= destSize) {
    text = text.substring(0, destSize - 4);
    text += "...";
  }
  text.toCharArray(dest, destSize);
}

bool fetchWorldNews(NewsItem outItems[], int &outCount) {
  outCount = 0;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("News: skipped, WiFi disconnected");
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(10000);
  http.setUserAgent("CloudAndCoin/" APP_VERSION);

  if (!http.begin(NEWS_FEED_URL)) {
    Serial.println("News: begin failed");
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("News: HTTP %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  if (payload.length() == 0) {
    Serial.println("News: empty feed");
    return false;
  }

  int pos = 0;
  while (outCount < MAX_NEWS_ITEMS) {
    int itemStart = payload.indexOf("<item", pos);
    if (itemStart < 0) break;
    int itemEnd = payload.indexOf("</item>", itemStart);
    if (itemEnd < 0) break;
    itemEnd += 7;

    String item = payload.substring(itemStart, itemEnd);
    String title = extractXmlTag(item, "title");
    String summary = extractXmlTag(item, "description");
    copyNewsText(outItems[outCount].title, sizeof(outItems[outCount].title), title);
    copyNewsText(outItems[outCount].summary, sizeof(outItems[outCount].summary), summary);

    if (outItems[outCount].title[0] != '\0') outCount++;
    pos = itemEnd;
  }

  Serial.printf("News: loaded %d item(s)\n", outCount);
  return outCount > 0;
}
