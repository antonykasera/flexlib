"""Writes the models in test/nn/models/ with TensorFlow's own converter, so that
nn-check sees what a real export holds: the operators, options and tensor
order TensorFlow chooses. It needs TensorFlow, which nn-check does not; run it
only to change a model, and commit the result.

    python3 test/nn/gen_models.py

Copyright (c) 2026 Antony Kasera
SPDX-License-Identifier: Apache-2.0
"""

from pathlib import Path

import numpy as np
import tensorflow as tf

OUT = Path(__file__).resolve().parent / "models"


def int8_model(model, shape, seed):
    """Converts a Keras model to full int8, inputs and outputs included."""
    rng = np.random.default_rng(seed)

    def representative():
        for _ in range(200):
            yield [rng.normal(0.0, 1.0, (1,) + shape).astype(np.float32)]
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = representative
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    return conv.convert()


def mlp_keras():
    """A small classifier: 4 x 16 features, flattened, two hidden layers."""
    tf.keras.utils.set_random_seed(1)
    model = tf.keras.Sequential([
        tf.keras.Input((4, 16)),
        tf.keras.layers.Flatten(),
        tf.keras.layers.Dense(32, activation="relu"),
        tf.keras.layers.Dense(16, activation="relu"),
        tf.keras.layers.Dense(4),
        tf.keras.layers.Softmax(),
    ])
    return int8_model(model, (4, 16), 2)


def cnn_keras():
    """A small image classifier with a residual add, an explicit pad before a
    strided convolution and global average pooling, the layers whose export
    matters."""
    tf.keras.utils.set_random_seed(3)
    L = tf.keras.layers
    x = tf.keras.Input((16, 16, 3))
    a = L.Conv2D(8, 3, padding="same", activation="relu")(x)
    a = L.MaxPooling2D(2)(a)
    b = L.DepthwiseConv2D(3, padding="same", activation="relu")(a)
    b = L.Conv2D(8, 1)(b)
    a = L.Add()([a, b])
    a = L.ZeroPadding2D(((1, 0), (1, 0)))(a)
    a = L.Conv2D(16, 3, strides=2, padding="valid", activation="relu")(a)
    a = L.GlobalAveragePooling2D()(a)
    a = L.Dense(5)(a)
    y = L.Softmax()(a)
    return int8_model(tf.keras.Model(x, y), (16, 16, 3), 4)


def main():
    OUT.mkdir(exist_ok=True)
    for name, make in (("mlp_keras", mlp_keras), ("cnn_keras", cnn_keras)):
        blob = make()
        (OUT / f"{name}.tflite").write_bytes(blob)
        print(f"{name}.tflite: {len(blob)} bytes")


if __name__ == "__main__":
    main()
