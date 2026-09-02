import os

Import("env")

version = os.environ.get(
    "CORONET_FIRMWARE_VERSION",
    env.GetProjectOption("custom_firmware_version", "0.0.1-dev"),
).strip()
if not version:
    version = "0.0.1-dev"

env.Append(CPPDEFINES=[("CORONET_FIRMWARE_VERSION", env.StringifyMacro(version))])
print(f"coroNET firmware version: {version}")
