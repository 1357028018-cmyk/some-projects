/*
 * snore_detector.h - C-compatible interface for snore detection
 *
 * Handles:
 *   1. Stereo PDM → Mono extraction (left channel only)
 *   2. Ring buffer accumulation (16000 mono samples = 1 second @ 16 kHz)
 *   3. Edge Impulse model inference via run_classifier()
 */

#ifndef SNORE_DETECTOR_H_
#define SNORE_DETECTOR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum classification label length */
#define SNORE_LABEL_MAX_LEN  32

/**
 * @brief Result of a single inference pass
 */
typedef struct {
    char    label[SNORE_LABEL_MAX_LEN];  /* predicted label */
    float   value;                        /* confidence [0.0, 1.0] */
} snore_result_t;

/**
 * @brief Initialize the snore detector.
 * Must be called once before any feed operations.
 * @return 0 on success, non-zero on failure
 */
int snore_detector_init(void);

/**
 * @brief Feed raw int16 mono audio samples into the detector.
 *
 * Data flow:
 *   PDM stereo (L0,R0,L1,R1...) → extract left channel →
 *   this function receives mono int16 → accumulate in ring buffer →
 *   when 16000 samples collected → run MFCC + inference
 *
 * @param samples  Pointer to mono int16 samples (already extracted from stereo)
 * @param count    Number of samples in the array
 */
void snore_detector_feed(const int16_t *samples, int count);

/**
 * @brief Get the latest inference result.
 * @param out  Pointer to receive the result
 * @return 1 if a fresh result is available, 0 if result is stale or no inference yet
 */
int snore_detector_get_result(snore_result_t *out);

/**
 * @brief Check if a new inference result is available since the last get_result() call.
 * @return 1 if new result available, 0 otherwise
 */
int snore_detector_has_new_result(void);

#ifdef __cplusplus
}
#endif

#endif /* SNORE_DETECTOR_H_ */
