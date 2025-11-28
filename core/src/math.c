#include "math.h"

// Simple sine approximation using Taylor series (limited range)
float sinf(float x) {
    // Normalize x to [-π, π]
    while (x > QARMA_PI) x -= QARMA_TAU;
    while (x < -QARMA_PI) x += QARMA_TAU;

    // Taylor series: sin(x) ≈ x - x^3/6 + x^5/120
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + x2 * x2 / 120.0f);
}

float cosf(float x) {
    return sinf(x + QARMA_PI / 2.0f);
}

float fabsf(float x) {
    return (x < 0.0f) ? -x : x;
}

float sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    float guess = x / 2.0f;
    for (int i = 0; i < 10; i++) {
        guess = 0.5f * (guess + x / guess);
    }
    return guess;
}


int abs(int x) {
    return (x < 0) ? -x : x;

}