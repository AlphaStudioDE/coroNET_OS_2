import os

Import("env")

version = os.environ.get(
    "CORONET_FIRMWARE_VERSION",
    env.GetProjectOption("custom_firmware_version", "1.0.0"),
).strip()
if not version:
    version = "1.0.0"

env.Append(CPPDEFINES=[("CORONET_FIRMWARE_VERSION", env.StringifyMacro(version))])
print(f"coroNET firmware version: {version}")
