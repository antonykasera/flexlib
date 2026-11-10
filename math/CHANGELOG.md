# flexmath changelog

## Unreleased

- Public headers moved to `math/include/`. No API change.

## 0.2.0 (2026-11-09)

- `fm_float`: sqrtf, recipf, rsqrtf, sinf / cosf / sincosf, atan2f / asinf / acosf, log2f / logf / exp2f / expf.
- `fm_linalg`: dot, cross, 3x3 / 4x4 matrix products in q31 and float.
- `fm_quat`: product, normalise, rotate, gyro integration.
- `fm_pid`: Q16.16 and float PID.
- No change to the 0.1.0 API.

## 0.1.0 (2026-11-09)

First release: `fm_fixed`, `fm_sqrt`, `fm_div`, `fm_trig`, `fm_atan`, `fm_exp`.
