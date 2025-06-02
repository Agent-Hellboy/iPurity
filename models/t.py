# convert_nsfw.py

import tensorflow as tf

# 1. Load the Keras H5 model without compiling (so optimizer config is ignored)
keras_model_path = "nsfw_mobilenet2.224x224.h5"
model = tf.keras.models.load_model(keras_model_path, compile=False)

# 2. Create a TFLiteConverter from the Keras model
converter = tf.lite.TFLiteConverter.from_keras_model(model)

# (Optional) Enable optimizations, quantization, etc. if desired.
# converter.optimizations = [tf.lite.Optimize.DEFAULT]

# 3. Convert to TFLite FlatBuffer format
tflite_model = converter.convert()

# 4. Save the converted model to 'models/nsfw_model.tflite'
tflite_output_path = "nsfw_model.tflite"
with open(tflite_output_path, "wb") as f:
    f.write(tflite_model)

print(f"✅ Converted to TFLite: {tflite_output_path}")

