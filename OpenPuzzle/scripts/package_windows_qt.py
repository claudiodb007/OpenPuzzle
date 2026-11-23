#!/usr/bin/env python3
"""Stage the Windows Qt distribution.

Use the build's matching Qt/MinGW SDK. Existing destinations are never replaced.
The engine ZIP is an explicitly selected, previously distributed OpenPuzzle ZIP;
only engines, license notices and puzzle metadata are imported from it.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--qt', type=Path, required=True)
    parser.add_argument('--compiler-runtime', type=Path, action='append', required=True)
    parser.add_argument('--engine-zip', type=Path, required=True)
    parser.add_argument('--qt-licenses', type=Path, required=True)
    parser.add_argument('--gpu-control-build-info', type=Path, required=True)
    parser.add_argument('--adlx-license', type=Path, required=True)
    parser.add_argument('--vanity-engine', type=Path,
                        help='Validated Windows x64 Vanity CUDA executable')
    parser.add_argument('--vanity-source-reference', type=Path,
                        help='Corresponding source and build information for the Vanity executable')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    controls = json.loads(args.gpu_control_build_info.read_text(encoding='utf-8-sig'))
    if (controls.get('platform') != 'windows' or controls.get('amd_adlx') is not True
            or controls.get('nvidia_nvml') is not True):
        parser.error('The Windows distribution requires both AMD ADLX and NVIDIA NVML support')
    if not args.adlx_license.is_file():
        parser.error('The AMD SDK license notice is required')
    vanity_license = source.parent / 'OpenPuzzle-Vanity' / 'LICENSE'
    if (args.vanity_engine is None) != (args.vanity_source_reference is None):
        parser.error('Provide both the Vanity engine and its corresponding source reference')
    if args.vanity_engine and not all(path.is_file() for path in
                                     (args.vanity_engine, args.vanity_source_reference, vanity_license)):
        parser.error('The Vanity executable, source reference and license must be existing files')
    if args.output.exists():
        parser.error('Destination already exists; preserve it and choose a new name')
    required = [args.build / 'platform' / n for n in
                ('openpuzzle.exe', 'openpuzzle-desktop.exe', 'openpuzzle-ui.exe', 'openpuzzle-gpu-control.exe')]
    required += [args.qt / 'bin' / (n + '.dll') for n in ('Qt6Core', 'Qt6Gui', 'Qt6Widgets')]
    required += [args.qt / 'plugins/platforms/qwindows.dll']
    required += [args.qt_licenses / n for n in ('LGPL-3.0-only.txt', 'GPL-3.0-only.txt', 'Qt-GPL-exception-1.0.txt')]
    if not (args.qt / 'sbom/qtbase-6.8.3.spdx.json').is_file():
        parser.error('The packager requires the verified Qt 6.8.3 SDK')
    if not any((p / 'COPYRIGHT.txt').is_file() for p in args.compiler_runtime):
        parser.error('The compiler runtime copyright notice is required')
    for path in required:
        if not path.is_file():
            parser.error(f'Missing required input: {path}')
    runtime = {}
    for folder in args.compiler_runtime:
        for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
            if (folder / name).is_file():
                runtime[name] = folder / name
    if len(runtime) != 3:
        parser.error('The matching MinGW compiler runtime DLLs are required')
    args.output.mkdir(parents=True)

    def copy(path, relative):
        target = args.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)

    for path in required[:4] + required[4:7]:
        copy(path, path.name)
    copy(required[7], 'platforms/qwindows.dll')
    copy(source / 'resources/windows/Diagnostico-GPU.cmd', 'Diagnostico-GPU.cmd')
    copy(args.gpu_control_build_info, 'gpu-control-build-info.json')
    copy(args.adlx_license, 'licenses/amd-adlx/ADLX SDK License Agreement.pdf')
    for name, path in runtime.items():
        copy(path, name)
    for folder in args.compiler_runtime:
        if (folder / 'COPYRIGHT.txt').is_file():
            copy(folder / 'COPYRIGHT.txt', 'licenses/compiler-runtime/COPYRIGHT.txt')
    shutil.copytree(args.qt_licenses, args.output / 'licenses/qt')
    shutil.copytree(source / 'resources/fonts', args.output / 'licenses/fonts',
                    ignore=shutil.ignore_patterns('*.ttf'))
    shutil.copytree(args.qt / 'sbom', args.output / 'licenses/qt/sbom')
    with zipfile.ZipFile(args.engine_zip) as archive:
        for item in archive.infolist():
            parts = PurePosixPath(item.filename.replace('\\', '/')).parts
            if item.is_dir() or len(parts) < 3 or parts[1] not in ('engines', 'licenses', 'puzzles'):
                continue
            if any(part in ('.', '..', '') for part in parts):
                parser.error('Unsafe archive member')
            destination = args.output.joinpath(*parts[1:])
            if destination.exists():
                parser.error(f'Archive would overwrite {destination}')
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(archive.read(item))
    for name in ('cuBitCrack.exe', 'clBitCrack.exe', 'keyhunt.exe'):
        if not (args.output / 'engines' / name).is_file():
            parser.error(f'Missing engine: {name}')
    if args.vanity_engine:
        for source_file, destination in (
            (args.vanity_engine, 'engines/vanity-offline.exe'),
            (vanity_license, 'licenses/vanity/LICENSE'),
            (args.vanity_source_reference, 'licenses/vanity/SOURCE.txt'),
        ):
            if (args.output / destination).exists():
                parser.error(f'Archive already contains {destination}')
            copy(source_file, destination)
    (args.output / 'qt.conf').write_text('[Paths]\nPlugins=.\n', encoding='utf-8')
    (args.output / 'Start.cmd').write_bytes(b'@echo off\r\nstart "" "%~dp0openpuzzle-ui.exe"\r\n')
    copy(source / 'docs/WINDOWS_QUICK_START_PT.md', 'README-PT.md')
    hashes = {str(p.relative_to(args.output)).replace('\\', '/'): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(args.output.rglob('*')) if p.is_file()}
    (args.output / 'SHA256SUMS.json').write_text(json.dumps(hashes, indent=2) + '\n', encoding='utf-8')
    print(f'Staged {len(hashes)} files: {args.output}')


if __name__ == '__main__':
    main()
