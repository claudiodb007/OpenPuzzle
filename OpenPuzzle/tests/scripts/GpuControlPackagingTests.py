#!/usr/bin/env python3
"""Exercise real install/CPack rules in a private tree; never run a GPU helper."""
import argparse
import io
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
import tempfile
import xml.etree.ElementTree as ET


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def run(args, **kwargs):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, **kwargs).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--cpack', required=True)
    options = parser.parse_args()
    source = options.source.resolve()
    dpkg = shutil.which('dpkg-deb')
    require(dpkg, 'dpkg-deb is required to inspect the Linux package')
    with tempfile.TemporaryDirectory(prefix='openpuzzle-package-') as temporary:
        root = Path(temporary)
        fixture = root / 'source'
        build = root / 'build'
        packages = root / 'packages'
        stage = root / 'stage'
        fixture.mkdir()
        # Bracket quoting keeps source paths literal, including spaces.
        fixture.joinpath('CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.22)
project(OpenPuzzle VERSION 1.0.46 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
enable_testing()
include(GNUInstallDirs)
add_subdirectory([==[%s/gpu-control]==] gpu-control)
include([==[%s/gpu-control/Install.cmake]==])
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_PACKAGE_NAME "OpenPuzzle")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenPuzzle package tests")
include(CPack)
''' % (source.as_posix(), source.as_posix()))
        run([options.cmake, '-S', str(fixture), '-B', str(build),
             '-DCMAKE_BUILD_TYPE=Release'])
        run([options.cmake, '--build', str(build), '--target',
             'OpenPuzzleGpuControlHelper', '--parallel', '2'])
        helper = build / 'gpu-control/openpuzzle-gpu-control'
        files = {
            'usr/libexec/openpuzzle-gpu-control': (helper.read_bytes(), 0o755),
            'usr/share/polkit-1/actions/org.openpuzzle.gpu-control.policy':
                ((source / 'packaging/polkit/org.openpuzzle.gpu-control.policy').read_bytes(), 0o644),
            'usr/share/doc/OpenPuzzle/scripts/install-gpu-control-helper.sh':
                ((source / 'scripts/install-gpu-control-helper.sh').read_bytes(), 0o755),
            'usr/share/doc/OpenPuzzle/docs/GPU_CONTROLS.md':
                ((source / 'docs/GPU_CONTROLS.md').read_bytes(), 0o644),
        }
        environment = os.environ.copy()
        environment['DESTDIR'] = str(stage)
        run([options.cmake, '--install', str(build), '--prefix', '/usr'], env=environment)
        for name, (data, mode) in files.items():
            target = stage / name
            require(target.is_file() and not target.is_symlink(), 'Missing installed file: ' + name)
            require(target.read_bytes() == data, 'Installed bytes differ: ' + name)
            require(stat.S_IMODE(target.stat().st_mode) == mode, 'Unsafe installed permissions: ' + name)
        require({p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()} == set(files),
                'Unexpected files in the GPU component installation')
        policy = ET.fromstring(files['usr/share/polkit-1/actions/org.openpuzzle.gpu-control.policy'][0])
        actions = policy.findall('action')
        require(len(actions) == 1 and actions[0].get('id') == 'org.openpuzzle.gpu-control',
                'Unexpected polkit actions')
        action = actions[0]
        require(action.findtext('defaults/allow_any') == 'no'
                and action.findtext('defaults/allow_inactive') == 'no'
                and action.findtext('defaults/allow_active') == 'auth_admin',
                'GPU Apply must require administrator authentication')
        require([(a.get('key'), a.text) for a in action.findall('annotate')] == [
            ('org.freedesktop.policykit.exec.path', '/usr/libexec/openpuzzle-gpu-control')],
                'Polkit must authorize only the dedicated helper')
        run([options.cpack, '--config', str(build / 'CPackConfig.cmake'), '-B', str(packages)])

        def inspect(archive, prefix, require_root=False):
            entries = {m.name.removeprefix('./').rstrip('/'): m for m in archive.getmembers() if m.isfile()}
            expected = {prefix + n for n in files}
            require(set(entries) == expected, 'Unexpected or missing package files: ' + repr(set(entries) ^ expected))
            require(not any(m.issym() or m.islnk() for m in archive.getmembers()), 'Package contains a link')
            for name, (data, mode) in files.items():
                member = entries[prefix + name]
                require(archive.extractfile(member).read() == data, 'Package bytes differ: ' + name)
                require(stat.S_IMODE(member.mode) == mode, 'Unsafe package permissions: ' + name)
                if require_root:
                    require(member.uid == 0 and member.gid == 0, 'DEB program files must be root-owned')

        tgzs = list(packages.glob('*.tar.gz'))
        debs = list(packages.glob('*.deb'))
        require(len(tgzs) == 1 and len(debs) == 1, 'Expected one TGZ and one DEB')
        with tarfile.open(tgzs[0]) as archive:
            marker = next(m.name for m in archive.getmembers() if m.name.endswith('/usr/libexec/openpuzzle-gpu-control'))
            inspect(archive, marker[:-len('usr/libexec/openpuzzle-gpu-control')])
        with tarfile.open(fileobj=io.BytesIO(run([dpkg, '--fsys-tarfile', str(debs[0])])), mode='r:') as archive:
            inspect(archive, '', require_root=True)
        with tarfile.open(fileobj=io.BytesIO(run([dpkg, '--ctrl-tarfile', str(debs[0])])), mode='r:') as archive:
            names = {Path(m.name).name for m in archive.getmembers()}
            require(not names & {'preinst', 'postinst', 'prerm', 'postrm'}, 'GPU component must not run install/removal hooks')
        print('GpuControlPackagingTests passed: real staged install, TGZ/DEB contents, '
              'root-owned helper, restricted polkit and no install hooks; no GPU accessed')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stdout.decode(errors='replace'))
        raise
