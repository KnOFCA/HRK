#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Universal HarmonyOS (DevEco Studio + hvigor) build driver.

What it does
------------
1. Locates a DevEco Studio installation (``--deveco``, env vars, common paths).
2. Locates the project root (``--project``, or an upward search for
   ``build-profile.json5`` + ``hvigorfile.ts``).
3. Reads ``build-profile.json5`` to pick the product and the
   ``module@target`` pairs to build.
4. On Windows, repairs ``oh_modules`` reparse points that the OS refuses to
   traverse.  A junction created by a process *without* the RedirectionGuard
   (``EnforceRedirectionTrust``) mitigation is "untrusted", so any
   RedirectionGuard-enabled process -- including an SSH session -- gets
   ``WinError 448`` / "The path cannot be traversed because it contains an
   untrusted mount point".  hvigor surfaces that as
   ``UNKNOWN: unknown error, stat`` and crashes ``CompileArkTS``.  Re-creating
   the link from the current process marks it trusted for this process tree.
5. Runs ``ohpm install``, then one ``hvigorw.js`` invocation per module.

Examples
--------
    python harmony_build.py --deveco "D:\\ophm\\DevEco Studio"
    python harmony_build.py --project D:\\src\\app\\harmony
    python harmony_build.py --build-mode release --clean
    python harmony_build.py --module entry@default --task assembleHap
    python harmony_build.py --list                 # show detection, run nothing
    python harmony_build.py --task assembleApp -- --stacktrace
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Optional

IS_WINDOWS = os.name == 'nt'
PROJECT_MARKER = 'hvigorfile.ts'
PROFILE_NAME = 'build-profile.json5'
ARTIFACT_SUFFIXES = ('.hap', '.hsp', '.app', '.har')
PRUNE_DIRS = {'.git', '.hvigor', '.idea', 'node_modules', 'oh_modules'}


# --------------------------------------------------------------------------
# logging
# --------------------------------------------------------------------------
class Log:
    def __init__(self, quiet: bool = False, verbose: bool = False) -> None:
        self.quiet = quiet
        self.verbose = verbose
        self.errors = 0

    def step(self, msg: str) -> None:
        if not self.quiet:
            print(f'\n==> {msg}', flush=True)

    def info(self, msg: str) -> None:
        if not self.quiet:
            print(f'    {msg}', flush=True)

    def detail(self, msg: str) -> None:
        if self.verbose and not self.quiet:
            print(f'    {msg}', flush=True)

    def warn(self, msg: str) -> None:
        print(f'    [warn] {msg}', file=sys.stderr, flush=True)

    def error(self, msg: str) -> None:
        print(f'    [error] {msg}', file=sys.stderr, flush=True)


def render(cmd: list[str]) -> str:
    if IS_WINDOWS:
        return subprocess.list2cmdline(cmd)
    import shlex
    return shlex.join(cmd)


# --------------------------------------------------------------------------
# toolchain discovery
# --------------------------------------------------------------------------
def node_binary(deveco: Path) -> Path:
    relative = Path('node.exe') if IS_WINDOWS else Path('bin') / 'node'
    return deveco / 'tools' / 'node' / relative


def ohpm_binary(deveco: Path) -> Path:
    return deveco / 'tools' / 'ohpm' / 'bin' / ('ohpm.bat' if IS_WINDOWS else 'ohpm')


def hvigor_script(deveco: Path) -> Path:
    return deveco / 'tools' / 'hvigor' / 'bin' / 'hvigorw.js'


def toolchain_problems(deveco: Path) -> list[str]:
    """Return the list of missing pieces; empty means 'usable DevEco root'."""
    if not deveco.is_dir():
        return [f'not a directory: {deveco}']
    problems = []
    for label, rel in (('jbr', 'jbr'), ('sdk', 'sdk'),
                       ('node', 'tools/node'), ('ohpm', 'tools/ohpm/bin'),
                       ('hvigor', 'tools/hvigor/bin/hvigorw.js')):
        if not (deveco / rel).exists():
            problems.append(f'missing {label} ({deveco / rel})')
    return problems


def deveco_candidates() -> list[Path]:
    """Ordered candidate DevEco Studio roots, most explicit first."""
    cands: list[Path] = []

    for var in ('DEVECO_HOME', 'DEVECO_STUDIO_HOME', 'DEVECO_STUDIO_PATH'):
        value = os.environ.get(var)
        if value:
            cands.append(Path(value))
    # Derive from the toolchain variables the script itself would export.
    sdk = os.environ.get('DEVECO_SDK_HOME')
    if sdk:
        cands.append(Path(sdk).parent)
    java_home = os.environ.get('JAVA_HOME')
    if java_home and Path(java_home).name.lower() == 'jbr':
        cands.append(Path(java_home).parent)

    home = Path.home()
    if IS_WINDOWS:
        relatives = (
            r'Program Files\Huawei\DevEco Studio',
            r'Huawei\DevEco Studio',
            r'Program Files\DevEco Studio',
            r'Program Files\DevEco-Studio',
            r'ophm\DevEco Studio',
            r'DevEco Studio',
            r'DevEco-Studio',
        )
        for drive in 'CDEFGHIJK':
            root = Path(f'{drive}:\\')
            if not root.exists():
                continue
            for rel in relatives:
                cands.append(root / rel)
        cands.append(home / 'AppData' / 'Local' / 'Huawei' / 'DevEco Studio')
    elif sys.platform == 'darwin':
        cands += [Path('/Applications/DevEco-Studio.app/Contents'),
                  Path('/Applications/DevEco Studio.app/Contents'),
                  home / 'Applications' / 'DevEco-Studio.app' / 'Contents']
    else:
        cands += [Path('/opt/deveco-studio'), Path('/opt/DevEco-Studio'),
                  Path('/usr/local/deveco-studio'),
                  home / 'DevEco-Studio', home / 'deveco-studio']

    seen, unique = set(), []
    for c in cands:
        key = str(c)
        if key not in seen:
            seen.add(key)
            unique.append(c)
    return unique


def resolve_deveco(explicit: Optional[str], log: Log) -> Optional[Path]:
    if explicit:
        path = Path(explicit).expanduser()
        problems = toolchain_problems(path)
        if problems:
            log.error(f'--deveco is not a usable DevEco Studio root: {path}')
            for p in problems:
                log.error(f'  {p}')
            return None
        return path.resolve()

    near_misses = []
    for cand in deveco_candidates():
        if cand.is_dir() and not toolchain_problems(cand):
            log.detail(f'deveco: {cand} (detected)')
            return cand.resolve()
        elif cand.is_dir():
            near_misses.append((cand, toolchain_problems(cand)))

    log.error('could not auto-detect DevEco Studio; pass --deveco PATH')
    if near_misses:
        log.error('candidates found but incomplete:')
        for cand, problems in near_misses[:5]:
            log.error(f'  {cand}: {"; ".join(problems)}')
    log.error('hint: set DEVECO_HOME, or pass --deveco "/path/to/DevEco Studio"')
    return None


# --------------------------------------------------------------------------
# project discovery
# --------------------------------------------------------------------------
def looks_like_project(path: Path) -> bool:
    return (path.is_dir()
            and (path / PROJECT_MARKER).is_file()
            and (path / PROFILE_NAME).is_file())


def resolve_project(explicit: Optional[str], script_dir: Path, log: Log) -> Optional[Path]:
    if explicit:
        path = Path(explicit).expanduser()
        if not looks_like_project(path):
            log.error(f'--project is not a HarmonyOS project root: {path}')
            log.error(f'  (expected both {PROJECT_MARKER} and {PROFILE_NAME})')
            return None
        return path.resolve()

    env = os.environ.get('HARMONY_PROJECT')
    if env and looks_like_project(Path(env)):
        return Path(env).resolve()

    roots: list[Path] = []
    cwd = Path.cwd().resolve()
    roots += [cwd, cwd / 'app' / 'harmony', cwd / 'harmony']
    # The original script lived in <project>/tools/, i.e. two levels below root.
    roots += [script_dir.parent, script_dir.parent / 'app' / 'harmony', script_dir]
    roots += list(cwd.parents)
    roots += list(script_dir.parents)

    for root in roots:
        if looks_like_project(root):
            log.detail(f'project: {root} (detected)')
            return root.resolve()

    log.error('could not auto-detect the project root; pass --project PATH')
    log.error(f'  (a project root contains {PROJECT_MARKER} and {PROFILE_NAME})')
    return None


# --------------------------------------------------------------------------
# build-profile.json5 (JSON5-lite)
# --------------------------------------------------------------------------
def strip_json5(text: str) -> str:
    """Remove // and /* */ comments plus trailing commas, keeping strings."""
    out: list[str] = []
    i, n = 0, len(text)
    quote = ''
    while i < n:
        ch = text[i]
        if quote:
            out.append(ch)
            if ch == '\\' and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == quote:
                quote = ''
            i += 1
            continue
        if ch in '"\'':
            quote = ch
            out.append(ch)
            i += 1
            continue
        if ch == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] not in '\r\n':
                i += 1
            continue
        if ch == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            continue
        out.append(ch)
        i += 1
    return re.sub(r',(\s*[}\]])', r'\1', ''.join(out))


def load_profile(root: Path) -> dict:
    path = root / PROFILE_NAME
    if not path.is_file():
        return {}
    try:
        text = path.read_text(encoding='utf-8-sig', errors='replace')
    except OSError:
        return {}
    for candidate in (text, strip_json5(text)):
        try:
            data = json.loads(candidate)
            if isinstance(data, dict):
                return data
        except ValueError:
            continue
    return {}


def profile_products(profile: dict) -> list[str]:
    try:
        products = profile['app']['products']
    except (KeyError, TypeError):
        return []
    return [p['name'] for p in products if isinstance(p, dict) and p.get('name')]


def profile_modules(profile: dict) -> list[tuple[str, str]]:
    """[(module_name, default_target)] in declaration order."""
    result: list[tuple[str, str]] = []
    modules = profile.get('modules')
    if not isinstance(modules, list):
        return result
    for module in modules:
        if not isinstance(module, dict) or not module.get('name'):
            continue
        targets = [t['name'] for t in (module.get('targets') or [])
                   if isinstance(t, dict) and t.get('name')]
        result.append((module['name'], targets[0] if targets else 'default'))
    return result


def discover_modules(root: Path) -> list[tuple[str, str]]:
    """Fallback: any direct child directory holding an hvigorfile.ts."""
    found = []
    try:
        children = sorted(root.iterdir())
    except OSError:
        return found
    for child in children:
        if child.is_dir() and (child / PROJECT_MARKER).is_file():
            found.append((child.name, 'default'))
    return found


# --------------------------------------------------------------------------
# Windows "untrusted mount point" repair
#
# Windows 11 marks a reparse point "trusted" based on the RedirectionGuard
# (EnforceRedirectionTrust) mitigation of the process that created it, not on
# ownership.  A junction produced by a non-RedirectionGuard process (e.g.
# DevEco Studio started from Explorer) is therefore invisible to a
# RedirectionGuard-enabled process such as an sshd child: lstat succeeds while
# stat/opendir fail with WinError 448.  The cure is to recreate the link from
# the process that needs to traverse it.
# --------------------------------------------------------------------------
def oh_modules_dirs(root: Path) -> list[Path]:
    dirs = []
    direct = root / 'oh_modules'
    if direct.is_dir():
        dirs.append(direct)
    try:
        children = sorted(root.iterdir())
    except OSError:
        return dirs
    for child in children:
        if child.is_dir() and (child / 'oh_modules').is_dir():
            dirs.append(child / 'oh_modules')
    return dirs


def reparse_target(link: Path) -> Optional[str]:
    try:
        target = os.readlink(link)
    except OSError:
        return None
    for prefix in ('\\\\?\\', '\\??\\'):
        if target.startswith(prefix):
            target = target[len(prefix):]
    target = target.rstrip('\\/')
    return target or None


def is_traversable(path: Path) -> bool:
    try:
        os.listdir(path)
        return True
    except OSError:
        return False


def scan_broken_links(root: Path) -> list[tuple[Path, str]]:
    """Reparse points under oh_modules that the OS cannot traverse."""
    broken = []
    for modules_dir in oh_modules_dirs(root):
        try:
            entries = sorted(modules_dir.iterdir())
        except OSError:
            continue
        for entry in entries:
            try:
                st = os.lstat(entry)
            except OSError:
                continue
            if not getattr(st, 'st_reparse_tag', 0):
                continue
            if is_traversable(entry):
                continue
            target = reparse_target(entry)
            if target:
                broken.append((entry, target))
    return broken


def link_exists(path: Path) -> bool:
    try:
        os.lstat(path)
        return True
    except OSError:
        return False


def repair_link(link: Path, target: str) -> bool:
    """Drop the link with rmdir (never touches the target) and re-create it."""
    if not Path(target).is_dir():
        return False
    dropped = subprocess.run(['cmd', '/c', 'rmdir', str(link)],
                             capture_output=True, text=True)
    if dropped.returncode != 0 or link_exists(link):
        return False
    created = subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), target],
                             capture_output=True, text=True)
    return created.returncode == 0 and is_traversable(link)


def preflight_links(root: Path, repair: bool, log: Log) -> None:
    if not IS_WINDOWS:
        return
    broken = scan_broken_links(root)
    if not broken:
        log.detail('oh_modules reparse points: all traversable')
        return
    for link, target in broken:
        log.warn(f'untraversable mount point: {link}')
        log.warn(f'  -> {target}')
        if not repair:
            continue
        if repair_link(link, target):
            log.info(f'repaired mount point: {link.name} -> {target}')
        else:
            log.error(f'could not repair {link}; CompileArkTS will likely fail')
    if not repair:
        log.warn('re-run with --repair-links to fix these automatically')


# --------------------------------------------------------------------------
# build
# --------------------------------------------------------------------------
def build_env(deveco: Path) -> dict:
    env = dict(os.environ)
    env['JAVA_HOME'] = str(deveco / 'jbr')
    env['DEVECO_SDK_HOME'] = str(deveco / 'sdk')
    env['PATH'] = os.pathsep.join([
        str(deveco / 'tools' / 'node'),
        str(deveco / 'tools' / 'ohpm' / 'bin'),
        env.get('PATH', ''),
    ])
    return env


def hvigor_command(deveco: Path, mode: str, product: str, module: Optional[str],
                   tasks: list[str], build_mode: Optional[str],
                   no_daemon: bool, extra: list[str]) -> list[str]:
    cmd = [str(node_binary(deveco)), str(hvigor_script(deveco)), '--mode', mode,
           '-p', f'product={product}']
    if mode == 'module' and module:
        cmd += ['-p', f'module={module}']
    if build_mode:
        cmd += ['-p', f'buildMode={build_mode}']
    cmd += tasks
    if no_daemon:
        cmd += ['--no-daemon']
    cmd += extra
    return cmd


def check_toolchain_files(deveco: Path, log: Log) -> bool:
    ok = True
    for label, path in (('node', node_binary(deveco)), ('ohpm', ohpm_binary(deveco)),
                        ('hvigorw.js', hvigor_script(deveco))):
        if not path.exists():
            log.error(f'{label} not found: {path}')
            ok = False
    return ok


def find_artifacts(root: Path) -> list[Path]:
    found = []
    for base, dirnames, filenames in os.walk(root, onerror=lambda e: None):
        dirnames[:] = [d for d in dirnames if d not in PRUNE_DIRS]
        if 'outputs' not in Path(base).parts:
            continue
        for name in filenames:
            if Path(name).suffix.lower() in ARTIFACT_SUFFIXES:
                found.append(Path(base) / name)
    return sorted(found)


def run_command(cmd: list[str], cwd: Path, env: dict, log: Log, dry_run: bool) -> int:
    log.info(f'$ {render(cmd)}')
    if dry_run:
        return 0
    try:
        return subprocess.run(cmd, cwd=str(cwd), env=env).returncode
    except OSError as error:
        log.error(f'failed to start {cmd[0]}: {error}')
        return 127


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def parse_args(argv: Optional[list[str]] = None):
    parser = argparse.ArgumentParser(
        prog='harmony_build.py',
        description='Universal DevEco Studio / hvigor build driver for HarmonyOS projects.',
        epilog='Unrecognised arguments are forwarded verbatim to hvigorw.js '
               '(e.g. -- --stacktrace).')
    parser.add_argument('--deveco', metavar='PATH',
                        help='DevEco Studio root (contains jbr/, sdk/, tools/). '
                             'Auto-detected when omitted.')
    parser.add_argument('--project', metavar='PATH',
                        help='project root containing hvigorfile.ts and '
                             'build-profile.json5. Auto-detected when omitted.')
    parser.add_argument('--product', metavar='NAME',
                        help='product name (default: first in build-profile.json5)')
    parser.add_argument('--module', metavar='NAME[@TARGET]', action='append',
                        help='module to build; repeatable. '
                             'Default: every module in build-profile.json5.')
    parser.add_argument('--task', metavar='NAME', action='append',
                        help='hvigor task; repeatable (default: assembleHap)')
    parser.add_argument('--mode', choices=('module', 'project'), default='module',
                        help='hvigor --mode (default: module)')
    parser.add_argument('--build-mode', choices=('debug', 'release'),
                        help='pass -p buildMode=<value>')
    parser.add_argument('--clean', action='store_true',
                        help='run the hvigor clean task before building')
    parser.add_argument('--skip-install', action='store_true',
                        help='skip ohpm install')
    parser.add_argument('--daemon', action='store_true',
                        help='allow the hvigor daemon (default: --no-daemon)')
    parser.add_argument('--repair-links', dest='repair_links', action='store_true',
                        default=True,
                        help='repair untraversable oh_modules mount points on '
                             'Windows (default)')
    parser.add_argument('--no-repair-links', dest='repair_links',
                        action='store_false',
                        help='only warn about untraversable mount points')
    parser.add_argument('--list', action='store_true',
                        help='print detected configuration and commands, then exit')
    parser.add_argument('--dry-run', action='store_true',
                        help='print commands without executing them')
    parser.add_argument('-q', '--quiet', action='store_true')
    parser.add_argument('-v', '--verbose', action='store_true')
    return parser.parse_known_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args, extra = parse_args(argv)
    log = Log(quiet=args.quiet, verbose=args.verbose)

    script_dir = Path(__file__).resolve().parent
    log.step('Resolving toolchain')
    deveco = resolve_deveco(args.deveco, log)
    if deveco is None:
        return 2
    log.info(f'DevEco Studio    : {deveco}')
    if not check_toolchain_files(deveco, log):
        return 2

    log.step('Resolving project')
    root = resolve_project(args.project, script_dir, log)
    if root is None:
        return 2
    log.info(f'project root     : {root}')

    profile = load_profile(root)
    products = profile_products(profile)
    modules = profile_modules(profile)
    if not modules:
        modules = discover_modules(root)
        if modules:
            log.detail('modules discovered from directory layout')

    product = args.product or (products[0] if products else 'default')
    if args.product and products and args.product not in products:
        log.warn(f'product "{args.product}" is not declared in {PROFILE_NAME} '
                 f'(declared: {", ".join(products)})')
    elif not args.product and products:
        log.detail(f'product "{product}" taken from {PROFILE_NAME}')
    elif not args.product:
        log.warn(f'no products declared in {PROFILE_NAME}; using "{product}"')

    target_by_module = dict(modules)
    if args.module:
        selected = []
        for spec in args.module:
            name, _, target = spec.partition('@')
            selected.append((name, target or target_by_module.get(name, 'default')))
    else:
        selected = list(modules) or [('entry', 'default')]

    tasks = args.task or ['assembleHap']
    mode = args.mode
    no_daemon = not args.daemon

    log.info(f'product          : {product}')
    log.info(f'modules          : {", ".join(f"{n}@{t}" for n, t in selected)}')
    log.info(f'tasks            : {" ".join(tasks)}   (mode={mode})')
    if args.build_mode:
        log.info(f'buildMode        : {args.build_mode}')
    if extra:
        log.info(f'extra hvigor args: {" ".join(extra)}')

    log.step('Preflight')
    preflight_links(root, args.repair_links, log)

    env = build_env(deveco)
    log.detail(f'JAVA_HOME       = {env["JAVA_HOME"]}')
    log.detail(f'DEVECO_SDK_HOME = {env["DEVECO_SDK_HOME"]}')

    commands: list[list[str]] = []
    if not args.skip_install:
        commands.append([str(ohpm_binary(deveco)), 'install'])
    if args.clean:
        for name, target in selected:
            commands.append(hvigor_command(deveco, mode, product,
                                           f'{name}@{target}' if mode == 'module' else None,
                                           ['clean'], args.build_mode, no_daemon, extra))
    if mode == 'project':
        commands.append(hvigor_command(deveco, mode, product, None, tasks,
                                       args.build_mode, no_daemon, extra))
    else:
        for name, target in selected:
            commands.append(hvigor_command(deveco, mode, product, f'{name}@{target}',
                                           tasks, args.build_mode, no_daemon, extra))

    log.step('Commands')
    for cmd in commands:
        log.info(f'$ {render(cmd)}')

    if args.list:
        return 0
    if args.dry_run:
        log.step('Dry run: nothing executed')
        return 0

    log.step('Building')
    for index, cmd in enumerate(commands, 1):
        log.info(f'[{index}/{len(commands)}] {Path(cmd[0]).name}')
        code = run_command(cmd, root, env, log, dry_run=False)
        if code != 0:
            log.error(f'command failed with exit code {code}')
            return code or 1

    artifacts = find_artifacts(root)
    log.step('Artifacts')
    if artifacts:
        for path in artifacts:
            try:
                size = path.stat().st_size
            except OSError:
                size = 0
            log.info(f'{path}  ({size:,} bytes)')
    else:
        log.info('(no .hap/.hsp/.app/.har found under an outputs/ directory)')

    log.step('BUILD SUCCEEDED')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print('\ninterrupted', file=sys.stderr)
        sys.exit(130)
