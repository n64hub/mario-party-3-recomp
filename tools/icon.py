"""Rasterise the program's icon, icons/icon.svg, into the files the builds use.

Usage: python -I tools/icon.py --chrome <chrome or chromium>
Needs Pillow.

icons/icon.svg is drawn for the project from plain shapes, so nothing in it comes from the game or its box. A headless
Chrome draws it, and this writes

    icons/512.png    the launcher's thumbnail (the game's menu, src/main/main.cpp)
    icons/app.ico    the executable's icon on Windows, in every size Explorer and the taskbar ask for (icons/app.rc);
                     SDL gives it to the window too
    icons/icon.bmp   the window's icon on Linux (SDL_SetWindowIcon in src/main/main.cpp), which has none without it
"""
import argparse
import os
import subprocess
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONS = os.path.join(ROOT, "icons")


def rasterise(chrome, svg_path, size):
    with tempfile.TemporaryDirectory() as tmp:
        png = os.path.join(tmp, "icon.png")
        page = os.path.join(tmp, "icon.html")
        with open(page, "w") as f:
            f.write(f'<html><body style="margin:0"><img src="file:///{svg_path.replace(os.sep, "/")}" '
                    f'width="{size}" height="{size}"></body></html>')
        subprocess.run([chrome, "--headless", "--disable-gpu", "--hide-scrollbars", "--mute-audio",
                        f"--user-data-dir={os.path.join(tmp, 'profile')}", "--default-background-color=00000000",
                        f"--window-size={size},{size}", f"--screenshot={png}", "file:///" + page.replace(os.sep, "/")],
                       check=True, capture_output=True)
        return Image.open(png).convert("RGBA").copy()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--chrome", required=True, help="a Chrome or Chromium to rasterise the icon with")
    args = parser.parse_args()
    big = rasterise(args.chrome, os.path.join(ICONS, "icon.svg"), 512)
    big.save(os.path.join(ICONS, "512.png"), optimize=True)
    # Each size is drawn down from the 512, so the small ones stay sharp.
    big.save(os.path.join(ICONS, "app.ico"), sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    # SDL reads a 32-bit BMP's fourth byte as alpha.
    big.resize((128, 128), Image.LANCZOS).save(os.path.join(ICONS, "icon.bmp"))


if __name__ == "__main__":
    main()
