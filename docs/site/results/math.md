# flexmath results

Every flexmath case and its baselines, by module, in cycles.
[Results](index.md) gives the conditions.

## Fixed point

[`fm_fixed.h`](../math/fm_fixed.md)

:::{bench-rows} fm_fixed.h
(float)q * 2^-15
(float)q * 2^-31
(q15)(x * 32768.0f)
(q31)(x * 2^31)
:::

## Square roots

[`fm_sqrt.h`](../math/fm_sqrt.md)

:::{bench-rows} fm_sqrt.h
arm_sqrt_
(uint32_t)sqrtf
sqrtf on Q16.16
1/sqrtf on Q16.16
:::

## Division

[`fm_div.h`](../math/fm_div.md)

:::{bench-rows} fm_div.h
arm_divide_
(int64_t)a * 65536 / b
2^32 / (int64_t)x
a / b in float
1 / x in float
:::

## Sine and cosine

[`fm_trig.h`](../math/fm_trig.md)

:::{bench-rows} fm_trig.h
arm_sin_q
:::

## Inverse trigonometry

[`fm_atan.h`](../math/fm_atan.md)

:::{bench-rows} fm_atan.h
arm_atan2_q
:::

## Logarithms and exponentials

[`fm_exp.h`](../math/fm_exp.md)

:::{bench-rows} fm_exp.h
arm_vlog_
exp2f via float
expf via float
log2f via float
logf via float
powf via float
:::

## Float functions

[`fm_float.h`](../math/fm_float.md)

:::{bench-rows} fm_float.h
sqrtf$
sqrtf, same inputs
1.0f / x
1.0f / sqrtf(x)
sinf$
cosf$
arm_sin_f32
arm_cos_f32
atan2f$
arm_atan2_f32
asinf$
acosf$
sinf, |x|
log2f, same
logf, same
exp2f, same
expf, same
:::

## Vectors and matrices

[`fm_linalg.h`](../math/fm_linalg.md)

:::{bench-rows} fm_linalg.h
arm_mat_mult_
dot3, float
cross3, float
mat3 mul, float
mat4 mul, float
:::

## Quaternions

[`fm_quat.h`](../math/fm_quat.md)

:::{bench-rows} fm_quat.h
arm_quaternion_
quat mul, float
quat rotate, float
quat normalize, 1 / sqrtf
quat integrate, float
:::

## PID

[`fm_pid.h`](../math/fm_pid.md)

:::{bench-rows} fm_pid.h
arm_pid_
pid f32, the same in plain float
:::
