#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include "esphome/core/log.h"

namespace eprices_periods {

static const char *TAG = "eprices_periods";

struct PeriodResult {
  int cheap1_start = -1, cheap1_end = -1;
  int cheap2_start = -1, cheap2_end = -1;
  int exp1_start = -1, exp1_end = -1;
  int exp2_start = -1, exp2_end = -1;
  std::string cheap_txt = "--";
  std::string exp_txt = "--";
};

// Window descriptor for precomputed averages
struct Window {
  int start;
  int end;  // exclusive
  float avg;
};

inline PeriodResult find_periods(const std::vector<float> &hourly_avg) {
  PeriodResult r;

  // Count valid hours
  int valid_hours = 0;
  for (int i = 0; i < 24 && i < (int)hourly_avg.size(); i++) {
    if (!std::isnan(hourly_avg[i])) valid_hours++;
  }
  if (valid_hours < 3) return r;

  // Precompute all valid window averages (sizes 3, 4, 5) in a single pass
  std::vector<Window> windows;
  windows.reserve(63);  // 22 + 21 + 20

  for (int sz = 3; sz <= 5; sz++) {
    for (int s = 0; s <= 24 - sz; s++) {
      float sum = 0;
      int cnt = 0;
      for (int j = s; j < s + sz; j++) {
        if (!std::isnan(hourly_avg[j])) { sum += hourly_avg[j]; cnt++; }
      }
      if (cnt < sz) continue;  // skip windows with NaN gaps
      windows.push_back({s, s + sz, sum / (float)cnt});
    }
  }

  if (windows.empty()) return r;

  // First pass: find best cheap and best expensive
  float best_cheap_avg = 9999.0f, best_exp_avg = -9999.0f;
  int cheap_idx = -1, exp_idx = -1;

  for (int i = 0; i < (int)windows.size(); i++) {
    if (windows[i].avg < best_cheap_avg) { best_cheap_avg = windows[i].avg; cheap_idx = i; }
    if (windows[i].avg > best_exp_avg) { best_exp_avg = windows[i].avg; exp_idx = i; }
  }

  if (cheap_idx >= 0) {
    r.cheap1_start = windows[cheap_idx].start;
    r.cheap1_end = windows[cheap_idx].end;
  }
  if (exp_idx >= 0) {
    r.exp1_start = windows[exp_idx].start;
    r.exp1_end = windows[exp_idx].end;
  }

  // Compute day price range for merge threshold
  float day_min = 9999.0f, day_max = -9999.0f;
  for (int i = 0; i < 24 && i < (int)hourly_avg.size(); i++) {
    if (!std::isnan(hourly_avg[i])) {
      if (hourly_avg[i] < day_min) day_min = hourly_avg[i];
      if (hourly_avg[i] > day_max) day_max = hourly_avg[i];
    }
  }
  float day_range = day_max - day_min;

  // Helper: find second-best non-overlapping, non-adjacent period
  // Only used when period1 is exactly 3 hours
  auto find_second = [&](int p1_start, int p1_end, int &p2_start, int &p2_end,
                         bool find_lowest) {
    p2_start = -1;
    p2_end = -1;
    float best2 = find_lowest ? 9999.0f : -9999.0f;
    for (int i = 0; i < (int)windows.size(); i++) {
      bool overlaps = !(windows[i].end <= p1_start || windows[i].start >= p1_end);
      bool adjacent = (windows[i].end == p1_start) || (windows[i].start == p1_end);
      if (overlaps || adjacent) continue;
      if ((find_lowest && windows[i].avg < best2) || (!find_lowest && windows[i].avg > best2)) {
        best2 = windows[i].avg;
        p2_start = windows[i].start;
        p2_end = windows[i].end;
      }
    }
  };

  // Helper: try to merge adjacent periods into a single larger window
  auto try_merge = [&](int &p1_start, int &p1_end, float p1_avg, float p2_avg,
                       bool find_lowest, int p2_start_orig, int p2_end_orig) {
    bool adjacent = (p1_end == p2_start_orig) || (p2_end_orig == p1_start);
    bool overlapping = !(p1_end <= p2_start_orig || p2_end_orig <= p1_start);
    if (!adjacent && !overlapping) return false;

    // Only merge if price difference is reasonable relative to day range
    float price_diff = std::fabs(p1_avg - p2_avg);
    if (day_range > 0.001f && price_diff > day_range * 0.5f) return false;

    int merged_start = std::min(p1_start, p2_start_orig);
    int merged_end = std::max(p1_end, p2_end_orig);

    // Find best window of max size (5) within the merged range
    int target_sz = std::min(merged_end - merged_start, 5);
    float best = find_lowest ? 9999.0f : -9999.0f;
    int best_s = merged_start;

    for (int s = merged_start; s + target_sz <= merged_end && s + target_sz <= 24; s++) {
      float sum = 0;
      bool valid = true;
      for (int j = s; j < s + target_sz; j++) {
        if (j >= (int)hourly_avg.size() || std::isnan(hourly_avg[j])) { valid = false; break; }
        sum += hourly_avg[j];
      }
      if (!valid) continue;
      float avg = sum / (float)target_sz;
      if ((find_lowest && avg < best) || (!find_lowest && avg > best)) {
        best = avg;
        best_s = s;
      }
    }

    p1_start = best_s;
    p1_end = best_s + target_sz;
    return true;
  };

  // For each type: find second period, then try merge if adjacent
  // Rule: use 2 periods only when period1 is 3h and they are not adjacent
  //       if period1 is 4-5h, use only one period
  auto resolve_periods = [&](int &p1_start, int &p1_end, int &p2_start, int &p2_end,
                             float p1_avg, bool find_lowest) {
    if (p1_start < 0) return;

    // If period1 is already 4-5h, no second period needed
    if ((p1_end - p1_start) >= 4) {
      p2_start = -1;
      p2_end = -1;
      return;
    }

    // Period1 is 3h: find best non-overlapping, non-adjacent second period
    float best2_avg = find_lowest ? 9999.0f : -9999.0f;
    int cand_start = -1, cand_end = -1;
    // Also track best adjacent candidate for potential merge
    float best_adj_avg = find_lowest ? 9999.0f : -9999.0f;
    int adj_start = -1, adj_end = -1;

    for (int i = 0; i < (int)windows.size(); i++) {
      bool overlaps = !(windows[i].end <= p1_start || windows[i].start >= p1_end);
      if (overlaps) continue;
      bool adjacent = (windows[i].end == p1_start) || (windows[i].start == p1_end);
      if (adjacent) {
        if ((find_lowest && windows[i].avg < best_adj_avg) || (!find_lowest && windows[i].avg > best_adj_avg)) {
          best_adj_avg = windows[i].avg;
          adj_start = windows[i].start;
          adj_end = windows[i].end;
        }
      } else {
        if ((find_lowest && windows[i].avg < best2_avg) || (!find_lowest && windows[i].avg > best2_avg)) {
          best2_avg = windows[i].avg;
          cand_start = windows[i].start;
          cand_end = windows[i].end;
        }
      }
    }

    // If best candidate is adjacent, try merging instead
    if (adj_start >= 0) {
      // Check if adjacent candidate is better than non-adjacent
      bool adj_better = (cand_start < 0) ||
                        (find_lowest && best_adj_avg < best2_avg) ||
                        (!find_lowest && best_adj_avg > best2_avg);
      if (adj_better) {
        float price_diff = std::fabs(p1_avg - best_adj_avg);
        if (day_range <= 0.001f || price_diff <= day_range * 0.5f) {
          // Merge: find best max-size window in combined range
          try_merge(p1_start, p1_end, p1_avg, best_adj_avg, find_lowest, adj_start, adj_end);
          p2_start = -1;
          p2_end = -1;
          return;
        }
      }
    }

    // Use non-adjacent second period
    p2_start = cand_start;
    p2_end = cand_end;
  };

  resolve_periods(r.cheap1_start, r.cheap1_end, r.cheap2_start, r.cheap2_end,
                  best_cheap_avg, true);
  resolve_periods(r.exp1_start, r.exp1_end, r.exp2_start, r.exp2_end,
                  best_exp_avg, false);

  // Build text strings
  r.cheap_txt = "";
  if (r.cheap1_start >= 0) {
    char b[14]; sprintf(b, "%02d:00-%02d:00", r.cheap1_start, r.cheap1_end);
    r.cheap_txt = b;
  }
  if (r.cheap2_start >= 0) {
    char b[14]; sprintf(b, "%02d:00-%02d:00", r.cheap2_start, r.cheap2_end);
    if (!r.cheap_txt.empty()) r.cheap_txt += ", ";
    r.cheap_txt += b;
  }
  if (r.cheap_txt.empty()) r.cheap_txt = "--";

  r.exp_txt = "";
  if (r.exp1_start >= 0) {
    char b[14]; sprintf(b, "%02d:00-%02d:00", r.exp1_start, r.exp1_end);
    r.exp_txt = b;
  }
  if (r.exp2_start >= 0) {
    char b[14]; sprintf(b, "%02d:00-%02d:00", r.exp2_start, r.exp2_end);
    if (!r.exp_txt.empty()) r.exp_txt += ", ";
    r.exp_txt += b;
  }
  if (r.exp_txt.empty()) r.exp_txt = "--";

  return r;
}

}  // namespace eprices_periods
