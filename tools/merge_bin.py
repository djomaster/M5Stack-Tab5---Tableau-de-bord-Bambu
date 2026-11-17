# Script PlatformIO (après compilation) : assemble bootloader + partitions + application en UN fichier
# « firmware-merged.bin » à flasher à l'adresse 0x0 (utile pour la publication sur GitHub).
# Ce script ne doit jamais faire échouer la compilation : en cas de problème il affiche un avertissement.
Import("env")


def merge_bin(source, target, env):
    try:
        import os
        import subprocess
        import sys

        platform = env.PioPlatform()
        esptool = os.path.join(platform.get_package_dir("tool-esptoolpy") or "", "esptool.py")
        build_dir = env.subst("$BUILD_DIR")
        app = str(target[0])
        out = os.path.join(build_dir, "firmware-merged.bin")
        images = []
        for off, path in env.get("FLASH_EXTRA_IMAGES", []):
            images += [env.subst(str(off)), env.subst(str(path))]
        app_offset = env.subst(env.get("ESP32_APP_OFFSET", "0x10000"))
        cmd = [env.subst("$PYTHONEXE"), esptool, "--chip", "esp32p4", "merge_bin", "-o", out,
               "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB"] + images + [app_offset, app]
        print("merge_bin : " + " ".join(cmd))
        subprocess.check_call(cmd)
        print("merge_bin : fichier unique créé : " + out)
    except Exception as e:  # ne jamais bloquer la compilation
        print("merge_bin : AVERTISSEMENT, fichier fusionné non créé (" + str(e) + ")")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
