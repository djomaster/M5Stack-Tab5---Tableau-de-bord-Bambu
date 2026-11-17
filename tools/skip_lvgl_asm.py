# Script PlatformIO : exclut de la compilation les fichiers assembleur de LVGL
# (accélérations ARM Helium/Neon) qui ne sont pas compatibles avec le processeur RISC-V de l'ESP32-P4
# et font échouer l'édition de liens ("can't link soft-float modules with single-float modules").
Import("env")


def skip_asm(node):
    path = node.get_path().replace("\\", "/")
    if path.endswith((".S", ".s")) and "/lvgl/" in path:
        return None
    return node


env.AddBuildMiddleware(skip_asm)
