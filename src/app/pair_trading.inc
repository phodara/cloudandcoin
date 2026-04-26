// ---------------- Pair trading ----------------
struct PairTradingCandidate {
  int a;
  int b;
  float correlation;
  float zScore;
  float score;
  char signal[8];
  char bias[24];
};

PairTradingCandidate pairTradingCandidates[PAIR_VISIBLE_ROWS];
int pairTradingCandidateCount = 0;

float pairTradingLatestPrice(int index) {
  if (index < 0 || index >= configuredCryptoCount) return NAN;
  if (!isnan(currentCryptoValues[index]) && currentCryptoValues[index] > 0.0f) return currentCryptoValues[index];
  if (cryptoHistoryOk[index] && cryptoHistory[index][HISTORY_POINTS - 1] > 0.0f) {
    return cryptoHistory[index][HISTORY_POINTS - 1];
  }
  return NAN;
}

bool pairTradingStatsForPair(int a, int b, float &correlation, float &zScore, float &score) {
  correlation = NAN;
  zScore = NAN;
  score = 0.0f;

  if (a < 0 || b < 0 || a >= configuredCryptoCount || b >= configuredCryptoCount) return false;
  if (!cryptoHistoryOk[a] || !cryptoHistoryOk[b]) return false;

  float sumA = 0.0f;
  float sumB = 0.0f;
  int returnCount = 0;

  for (int i = 1; i < HISTORY_POINTS; i++) {
    float a0 = cryptoHistory[a][i - 1];
    float a1 = cryptoHistory[a][i];
    float b0 = cryptoHistory[b][i - 1];
    float b1 = cryptoHistory[b][i];
    if (a0 <= 0.0f || a1 <= 0.0f || b0 <= 0.0f || b1 <= 0.0f) return false;

    float ra = logf(a1 / a0);
    float rb = logf(b1 / b0);
    sumA += ra;
    sumB += rb;
    returnCount++;
  }

  if (returnCount < 2) return false;

  float meanA = sumA / returnCount;
  float meanB = sumB / returnCount;
  float covariance = 0.0f;
  float varianceA = 0.0f;
  float varianceB = 0.0f;

  for (int i = 1; i < HISTORY_POINTS; i++) {
    float ra = logf(cryptoHistory[a][i] / cryptoHistory[a][i - 1]) - meanA;
    float rb = logf(cryptoHistory[b][i] / cryptoHistory[b][i - 1]) - meanB;
    covariance += ra * rb;
    varianceA += ra * ra;
    varianceB += rb * rb;
  }

  if (varianceA <= 0.0f || varianceB <= 0.0f) return false;
  correlation = covariance / sqrtf(varianceA * varianceB);

  float ratioSum = 0.0f;
  for (int i = 0; i < HISTORY_POINTS; i++) {
    ratioSum += logf(cryptoHistory[a][i] / cryptoHistory[b][i]);
  }

  float ratioMean = ratioSum / HISTORY_POINTS;
  float ratioVariance = 0.0f;
  for (int i = 0; i < HISTORY_POINTS; i++) {
    float centered = logf(cryptoHistory[a][i] / cryptoHistory[b][i]) - ratioMean;
    ratioVariance += centered * centered;
  }

  float ratioStd = sqrtf(ratioVariance / (HISTORY_POINTS - 1));
  if (ratioStd <= 0.000001f || isnan(ratioStd)) return false;

  float latestA = pairTradingLatestPrice(a);
  float latestB = pairTradingLatestPrice(b);
  if (isnan(latestA) || isnan(latestB) || latestA <= 0.0f || latestB <= 0.0f) return false;

  zScore = (logf(latestA / latestB) - ratioMean) / ratioStd;

  float positiveCorrelation = correlation > 0.0f ? correlation : 0.0f;
  score = fabsf(zScore) * positiveCorrelation;
  return !isnan(correlation) && !isnan(zScore);
}

void pairTradingDescribeSignal(float correlation, float zScore, char *signalOut, size_t signalOutSize) {
  float absZ = fabsf(zScore);
  const char *signal = "Weak";
  if (correlation >= 0.70f && absZ >= 2.0f) signal = "Strong";
  else if (correlation >= 0.55f && absZ >= 1.3f) signal = "Med";
  snprintf(signalOut, signalOutSize, "%s", signal);
}

void pairTradingDescribeBias(int a, int b, float zScore, char *biasOut, size_t biasOutSize) {
  if (fabsf(zScore) < 1.0f) {
    snprintf(biasOut, biasOutSize, "Watch");
  } else if (zScore < 0.0f) {
    snprintf(biasOut, biasOutSize, "Long %s", activeCryptos[a].symbol);
  } else {
    snprintf(biasOut, biasOutSize, "Long %s", activeCryptos[b].symbol);
  }
}

void pairTradingInsertCandidate(const PairTradingCandidate &candidate) {
  int insertAt = pairTradingCandidateCount;
  for (int i = 0; i < pairTradingCandidateCount; i++) {
    if (candidate.score > pairTradingCandidates[i].score) {
      insertAt = i;
      break;
    }
  }

  if (insertAt >= PAIR_VISIBLE_ROWS) return;

  int limit = pairTradingCandidateCount;
  if (limit >= PAIR_VISIBLE_ROWS) limit = PAIR_VISIBLE_ROWS - 1;
  for (int i = limit; i > insertAt; i--) {
    pairTradingCandidates[i] = pairTradingCandidates[i - 1];
  }

  pairTradingCandidates[insertAt] = candidate;
  if (pairTradingCandidateCount < PAIR_VISIBLE_ROWS) pairTradingCandidateCount++;
}

void pairTradingComputeCandidates() {
  pairTradingCandidateCount = 0;
  if (configuredCryptoCount < 2) return;

  for (int a = 0; a < configuredCryptoCount - 1; a++) {
    for (int b = a + 1; b < configuredCryptoCount; b++) {
      PairTradingCandidate candidate;
      candidate.a = a;
      candidate.b = b;
      if (!pairTradingStatsForPair(a, b, candidate.correlation, candidate.zScore, candidate.score)) {
        continue;
      }

      pairTradingDescribeSignal(candidate.correlation, candidate.zScore, candidate.signal, sizeof(candidate.signal));
      pairTradingDescribeBias(a, b, candidate.zScore, candidate.bias, sizeof(candidate.bias));
      pairTradingInsertCandidate(candidate);
    }
  }
}

void pairTradingRender() {
  if (!pair_title_label) return;

  lv_label_set_text(pair_title_label, "Pair Trading");
  lv_obj_set_style_text_color(pair_title_label, lv_color_hex(0xAAAAAA), 0);

  pairTradingComputeCandidates();

  if (configuredCryptoCount < 2) {
    for (int i = 0; i < PAIR_VISIBLE_ROWS; i++) {
      lv_obj_t *label = pair_value_labels[i];
      if (!label) continue;
      lv_label_set_text(label, i == 0 ? "Add 2+ crypto tickers" : "");
      lv_obj_set_style_text_color(label, lv_color_hex(0xAAAAAA), 0);
    }
    pairTradingDirty = false;
    return;
  }

  if (pairTradingCandidateCount == 0) {
    for (int i = 0; i < PAIR_VISIBLE_ROWS; i++) {
      lv_obj_t *label = pair_value_labels[i];
      if (!label) continue;
      lv_label_set_text(label, i == 0 ? "Waiting for history data" : "");
      lv_obj_set_style_text_color(label, lv_color_hex(0xAAAAAA), 0);
    }
    pairTradingDirty = false;
    return;
  }

  for (int i = 0; i < PAIR_VISIBLE_ROWS; i++) {
    lv_obj_t *label = pair_value_labels[i];
    if (!label) continue;

    if (i >= pairTradingCandidateCount) {
      lv_label_set_text(label, "");
      continue;
    }

    PairTradingCandidate &candidate = pairTradingCandidates[i];
    char text[96];
    snprintf(
      text,
      sizeof(text),
      "%s/%s %s z%.1f c%.2f %s",
      activeCryptos[candidate.a].symbol,
      activeCryptos[candidate.b].symbol,
      candidate.signal,
      candidate.zScore,
      candidate.correlation,
      candidate.bias
    );

    lv_label_set_text(label, text);
    uint32_t color = 0xCCCCCC;
    if (strcmp(candidate.signal, "Strong") == 0) color = 0x00FF00;
    else if (strcmp(candidate.signal, "Med") == 0) color = 0xF4B400;
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  }

  pairTradingDirty = false;
}
