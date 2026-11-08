#!/usr/bin/env python3
"""Build an experimental UI-only AppImage from a verified native UI build.

Tools are supplied explicitly; this script never downloads or installs anything.
Host libc/loader and graphics drivers stay on the host. Build on the oldest
target system you intend to support, then validate that desktop separately.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tarfile
import tempfile
import zlib


def run(*command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


def write_icon(path):
    """Rasterize the simple circle icon from its geometry; no external renderer."""
    pixels = bytearray()
    for y in range(256):
        pixels.append(0)  # PNG scanline filter
        for x in range(256):
            covered = white = 0
            for oy in (.25, .75):
                for ox in (.25, .75):
                    dx, dy = abs(x + ox - 128), abs(y + oy - 128)
                    if max(dx - 80, 0) ** 2 + max(dy - 80, 0) ** 2 <= 48 ** 2:
                        covered += 1
                        white += 52 ** 2 <= dx * dx + dy * dy <= 76 ** 2
            fraction = white / covered if covered else 0
            pixels.extend((round(255 * fraction), round(122 + 133 * fraction),
                           round(204 + 51 * fraction), round(255 * covered / 4)))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 256, 256, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--appimagetool", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if output.exists():
        parser.error("Output already exists; select a new path")
    for source in (args.binary, args.appimagetool, args.runtime):
        if not source.is_file():
            parser.error(f"Missing build input: {source}")
    appdir_parent = repo / "build" / "appimage"
    appdir_parent.mkdir(parents=True, exist_ok=True)
    appdir = Path(tempfile.mkdtemp(prefix="OpenKey-", suffix=".AppDir", dir=appdir_parent))
    appdir.chmod(0o755)
    prefix = appdir / "usr"
    for directory in ("bin", "lib", "share/doc/openkey-settings", "share/licenses"):
        (prefix / directory).mkdir(parents=True)
    binary = prefix / "bin/openkey-settings"
    shutil.copy2(args.binary, binary)
    for variable, destination in (("QT_INSTALL_PLUGINS", "plugins"), ("QT_INSTALL_QML", "qml")):
        source = Path(run("qmake6", "-query", variable, capture_output=True).stdout.strip())
        if not source.is_dir():
            raise RuntimeError(f"Qt directory missing: {source}")
        shutil.copytree(source, prefix / destination)
    (prefix / "bin/qt.conf").write_text("[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n")
    exclude = re.compile(r"^(ld-linux.*|lib(c|m|pthread|dl|rt|resolv|util|nss_.*)\.so.*)$")
    queue = [binary, *prefix.joinpath("plugins").rglob("*.so"), *prefix.joinpath("qml").rglob("*.so")]
    seen = set()
    package_paths = []
    while queue:
        elf = queue.pop()
        if elf.resolve() in seen:
            continue
        seen.add(elf.resolve())
        listing = run("ldd", str(elf), capture_output=True).stdout
        if "not found" in listing:
            raise RuntimeError(f"Missing library for {elf}: {listing}")
        for match in re.finditer(r"(?:=>\s+|^\s*)(/\S+)\s+\(", listing, re.MULTILINE):
            library = Path(match.group(1))
            if exclude.match(library.name):
                continue
            target = prefix / "lib" / library.name
            if not target.exists():
                shutil.copy2(library, target)
                package_paths.append(str(library.resolve()))
                queue.append(library)
    # Include distributor copyright notices for bundled libraries and Qt modules.
    query_paths = package_paths + [str(p) for p in Path(run("qmake6", "-query", "QT_INSTALL_QML", capture_output=True).stdout.strip()).rglob("qmldir")]
    query_paths += [str(p) for p in Path(run("qmake6", "-query", "QT_INSTALL_PLUGINS", capture_output=True).stdout.strip()).rglob("*.so")]
    packages = set()
    for start in range(0, len(query_paths), 100):
        reply = subprocess.run(["dpkg-query", "-S", *query_paths[start:start + 100]], text=True, capture_output=True)
        for line in reply.stdout.splitlines():
            if ": /" in line:
                packages.add(line.split(": /", 1)[0].split(":", 1)[0])
    for package in sorted(packages):
        copyright_file = Path("/usr/share/doc") / package / "copyright"
        if copyright_file.is_file():
            shutil.copy2(copyright_file, prefix / "share/licenses" / (package + ".copyright"))
        else:
            raise RuntimeError(f"Missing distributor copyright notice: {package}")
    (prefix / "share/licenses/BUNDLED_PACKAGES.txt").write_text("\n".join(sorted(packages)) + "\n")
    docs = prefix / "share/doc/openkey-settings"
    shutil.copy2(repo / "LICENSE", docs / "LICENSE")
    shutil.copy2(repo / "docs/UBUNTU_APPIMAGE.md", docs / "README.md")
    # Corresponding OpenKey sources, including current uncommitted changes.
    paths = run("git", "-C", str(repo), "ls-files", "-z", "--cached", "--others", "--exclude-standard", capture_output=True).stdout.split("\0")
    allowed = ("Sources/OpenKey/engine/", "Sources/OpenKey/linux/", "Tests/", "cmake/", "scripts/", "tools/", "docs/")
    with tarfile.open(docs / "source.tar.gz", "w:gz") as archive:
        for name in sorted(set(paths)):
            if name and (name in ("LICENSE", "CMakeLists.txt") or name.startswith(allowed)) and (repo / name).is_file():
                archive.add(repo / name, arcname="OpenKey-source/" + name, recursive=False)
    (appdir / "AppRun").write_text('''#!/bin/sh
set -eu
appdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$appdir/usr/lib"
export QT_PLUGIN_PATH="$appdir/usr/plugins"
export QML_IMPORT_PATH="$appdir/usr/qml"
export QML2_IMPORT_PATH="$appdir/usr/qml"
exec "$appdir/usr/bin/openkey-settings" "$@"
''')
    (appdir / "AppRun").chmod(0o755)
    (appdir / "openkey-settings.desktop").write_text("[Desktop Entry]\nType=Application\nName=OpenKey Settings\nExec=openkey-settings\nIcon=openkey-settings\nCategories=Settings;\n")
    (appdir / "openkey-settings.svg").write_text('<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128"><rect width="128" height="128" rx="24" fill="#007acc"/><circle cx="64" cy="64" r="32" fill="none" stroke="white" stroke-width="12"/></svg>')
    write_icon(appdir / "openkey-settings.png")
    (appdir / ".DirIcon").symlink_to("openkey-settings.png")
    environment = dict(os.environ, ARCH="x86_64", NO_APPSTREAM="1")
    # Validate the bundled executable before emitting the image; no session I/O.
    smoke = dict(environment, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software", QT_IM_MODULE="compose")
    run(str(appdir / "AppRun"), "--smoke-test", env=smoke)
    output.parent.mkdir(parents=True, exist_ok=True)
    run(str(args.appimagetool.resolve()), "--no-appstream", "--mksquashfs-opt", "-processors",
        "--mksquashfs-opt", "2", "--runtime-file", str(args.runtime.resolve()), str(appdir), str(output), env=environment)
    output.with_name(output.name + ".sha256").write_text(hashlib.sha256(output.read_bytes()).hexdigest() + "  " + output.name + "\n")
    print(f"AppDir: {appdir}\nAppImage: {output}")


if __name__ == "__main__":
    main()
