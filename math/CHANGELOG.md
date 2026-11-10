# flexmath changelog

## 0.2.1 (2026-11-10)

- Public headers moved to `math/include/`. No API change.
- `fm_pid`: anti-windup now stops the integrator at the output limit while the output is saturated; a saturating step no longer overshoots (it could by ~7%). 171 / 501 cycles (was 147 / 442).

## 0.2.0 (2026-11-09)

- `fm_float`: sqrtf, recipf, rsqrtf, sinf / cosf / sincosf, atan2f / asinf / acosf, log2f / logf / exp2f / expf.
- `fm_linalg`: dot, cross, 3x3 / 4x4 matrix products in q31 and float.
- `fm_quat`: product, normalise, rotate, gyro integration.
- `fm_pid`: Q16.16 and float PID.
- No change to the 0.1.0 API.

## 0.1.0 (2026-11-09)

First release: `fm_fixed`, `fm_sqrt`, `fm_div`, `fm_trig`, `fm_atan`, `fm_exp`.
