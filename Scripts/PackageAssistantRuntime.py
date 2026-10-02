"""Build a self-contained Windows assistant archive; maintainer tool, never run by users."""
from __future__ import annotations
import argparse, csv, hashlib, importlib.metadata, io, json, pathlib, shutil, subprocess, sys, tempfile, zipfile

PYTHON_VERSION = "3.13.16"
PYTHON_SHA256 = "97dae5274cc54867065e8d5a3226e48c35017ed332a0fdb0e27d5b5821961297"

def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python-zip", type=pathlib.Path, required=True)
    parser.add_argument("--core-wheel", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--constraints", type=pathlib.Path, help="Pinned dependency versions for a license-only rebuild")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    output = args.output or root / "Plugins/CyUEMCPSDK/Resources/AssistantRuntime"
    if digest(args.python_zip) != PYTHON_SHA256:
        raise ValueError("Portable CPython archive does not match the verified official SHA256")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cyue-package-") as work:
        stage = pathlib.Path(work) / "runtime"
        stage.mkdir()
        with zipfile.ZipFile(args.python_zip) as source:
            source.extractall(stage)
        site = stage / "Lib/site-packages"
        subprocess.run([sys.executable, "-m", "pip", "install", "--isolated", "--disable-pip-version-check", "--keyring-provider", "disabled",
                        "--target", str(site), "--platform", "win_amd64", "--implementation", "cp", "--python-version", "3.13", "--abi", "cp313",
                        "--only-binary=:all:", "--ignore-installed", "--no-compile",
                        *(["--constraint", str(args.constraints.resolve())] if args.constraints else []), str(args.core_wheel.resolve()),
                        "mcp==1.30.0", "Pillow>=11,<13", "pypdf>=6,<7"], check=True)
        # pip console launchers reference temporary build Python paths. The embedded
        # runtime starts companion/run.py directly and does not distribute these launchers.
        launchers = site / "bin"
        if launchers.exists():
            assert launchers.resolve().is_relative_to(stage.resolve())
            shutil.rmtree(launchers)
        for record in site.glob("*.dist-info/RECORD"):
            rows = list(csv.reader(io.StringIO(record.read_text(encoding="utf-8"))))
            rows = [row for row in rows if not row[0].startswith("../../bin/")]
            with record.open("w", encoding="utf-8", newline="") as stream:
                csv.writer(stream).writerows(rows)
        shutil.copytree(root / "Bridge/cyue_mcp", stage / "companion/cyue_mcp", ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        shutil.copyfile(root / "Bridge/run.py", stage / "companion/run.py")
        shutil.copyfile(root / "LICENSE", stage / "companion/LICENSE")
        shutil.copyfile(root / "LICENSE", output / "LICENSE-CyUEMCPSDK.txt")
        (stage / "python313._pth").write_text("python313.zip\n.\nLib/site-packages\nLib/site-packages/win32\nLib/site-packages/win32/lib\nLib/site-packages/Pythonwin\ncompanion\n", encoding="utf-8")
        for metadata in site.glob("*.dist-info/direct_url.json"):
            metadata.unlink()  # Build machine paths are not distribution metadata.
        for dll in (site / "pywin32_system32").glob("*.dll"):
            shutil.copyfile(dll, stage / dll.name)
        versions = {d.metadata["Name"]: d.version for d in importlib.metadata.distributions(path=[str(site)])}
        version = (root / "Bridge/cyue_mcp/__init__.py").read_text().split('"')[1]
        probe = "import sys, json, importlib.metadata as m; import cytools_core, mcp, PIL, pypdf, cyue_mcp.service; assert sys.version_info[:3] == (3,13,16); print(json.dumps({'python':sys.version.split()[0], 'core':m.version('cytools-core')}))"
        subprocess.run([str(stage / "python.exe"), "-B", "-c", probe], check=True)
        (stage / "verify-runtime.py").write_text("""import importlib.metadata as metadata
import json, pathlib, sys
import cytools_core, mcp, PIL, pypdf, cyue_mcp.service, pywintypes, win32api
import cyue_mcp
manifest = json.loads((pathlib.Path(__file__).parent / "runtime-manifest.json").read_text(encoding="utf-8"))
for name, version in manifest["packages"].items():
    assert metadata.version(name) == version, name
assert sys.version.split()[0] == manifest["pythonVersion"]
assert sys.flags.isolated and sys.dont_write_bytecode
print(json.dumps({"python": sys.version.split()[0], "sdk": cyue_mcp.__version__}))
""", encoding="utf-8")
        files = {p.relative_to(stage).as_posix(): digest(p) for p in sorted(stage.rglob("*")) if p.is_file()}
        inside = {"formatVersion": 1, "sdkVersion": version, "pythonVersion": PYTHON_VERSION, "platform": "win64", "abi": "cp313",
                  "pythonExecutable": "python.exe", "entryPoint": "companion/run.py", "packages": versions, "files": files,
                  "licenses": [p for p in files if "license" in p.lower() or "copying" in p.lower()]}
        (stage / "runtime-manifest.json").write_text(json.dumps(inside, indent=2, sort_keys=True), encoding="utf-8")
        archive = output / "assistant-win64-cp313.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    # Stable metadata makes identical payloads share the same cache key.
                    info = zipfile.ZipInfo(path.relative_to(stage).as_posix(), (2026, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    bundle.writestr(info, path.read_bytes(), compresslevel=9)
        manifest = {k: v for k, v in inside.items() if k not in ("files", "licenses")}
        manifest.update(archive=archive.name, sha256=digest(archive), pythonSha256=files["python.exe"],
                        runtimeManifestSha256=digest(stage / "runtime-manifest.json"), compressedBytes=archive.stat().st_size, extractedBytes=sum(p.stat().st_size for p in stage.rglob("*") if p.is_file()), fileCount=len(files)+1)
        licenses = ["Assistant runtime component notices", "", "Portable CPython " + PYTHON_VERSION + " | Python Software Foundation License", "CyUEMCPSDK " + version + " | MIT (see companion/LICENSE)", "", "Installed dependency metadata and license texts:"]
        for distribution in sorted(importlib.metadata.distributions(path=[str(site)]), key=lambda d: d.metadata["Name"].lower()):
            license_name = distribution.metadata.get("License-Expression") or distribution.metadata.get("License") or "See included component license"
            licenses.append(distribution.metadata["Name"] + " " + distribution.version + " | " + license_name)
        licenses += ["", "Full license files inside the archive and installed runtime:"] + inside["licenses"]
        (output / "Licenses.txt").write_text("\n".join(licenses) + "\n", encoding="utf-8")
        plugin_licenses = root / "Plugins/CyUEMCPSDK/LICENCES"
        plugin_licenses.mkdir(exist_ok=True)
        shutil.copyfile(root / "LICENSE", plugin_licenses / "CyUEMCPSDK-MIT.txt")
        shutil.copyfile(output / "Licenses.txt", plugin_licenses / "AssistantRuntime-Notices.txt")
        core_version = next(v for k, v in versions.items() if k.lower() == "cytools-core")
        shutil.copyfile(site / ("cytools_core-" + core_version + ".dist-info/licenses/LICENSE"),
                        plugin_licenses / "CyToolsCore-MIT.txt")
        (output / "bundle.json").write_text(json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8")
        print(json.dumps({"archive": str(archive), "compressedBytes": manifest["compressedBytes"], "extractedBytes": manifest["extractedBytes"], "sha256": manifest["sha256"]}))

if __name__ == "__main__":
    main()
