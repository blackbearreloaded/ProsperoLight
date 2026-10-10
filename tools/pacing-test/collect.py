#!/usr/bin/env python3
# ps5-native-app-boilerplate - Archive finished PS5 pacing test sessions.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Standard library only; run on the PC or another LAN computer."""
import argparse
import datetime
from ftplib import FTP
import json
from pathlib import Path
import time

LOGS = '/data/prosperolight/logs'


def completion_key(entries):
    """Only traces written at session completion trigger another archive."""
    return tuple(sorted((name, info.get('modify'), info.get('size'))
                        for name, info in entries
                        if info.get('type') == 'file' and
                        (name == 'performance-last.json' or
                         name.startswith(('native-decisions-', 'pyrowave-decisions-')))))


def snapshot(ftp, entries, output):
    output.mkdir(parents=True)
    files = []
    for name, info in entries:
        if info.get('type') != 'file' or Path(name).name != name:
            continue
        if not name.endswith(('.json', '.txt', '.csv', '.log')):
            continue
        with (output / name).open('wb') as target:
            ftp.retrbinary('RETR ' + LOGS + '/' + name, target.write)
        files.append(dict(name=name, **info))
    try:
        with (output / 'mounted-build-label.txt').open('wb') as target:
            ftp.retrbinary('RETR /mnt/sandbox/PPSA99002_000/app0/build-label.txt', target.write)
    except Exception as exc:
        print('Build label unavailable:', exc)
    (output / 'manifest.json').write_text(json.dumps(files, indent=2), encoding='utf-8')
    print('Saved:', output, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, default=2121)
    parser.add_argument('--output', type=Path, default=Path('pacing-logs'))
    parser.add_argument('--watch', action='store_true')
    parser.add_argument('--poll', type=float, default=5)
    args = parser.parse_args()
    if args.poll < 1:
        parser.error('--poll must be at least 1 second')
    saved = candidate = None
    while True:
        try:
            with FTP() as ftp:
                ftp.connect(args.host, args.port, timeout=20)
                ftp.login()
                entries = list(ftp.mlsd(LOGS))
                key = completion_key(entries)
                if not args.watch or (key == candidate and key != saved):
                    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
                    snapshot(ftp, entries, args.output / stamp)
                    saved = key
                candidate = key
            if not args.watch:
                return
        except Exception as exc:
            if not args.watch:
                raise
            print('FTP unavailable, retrying:', exc, flush=True)
            candidate = None
        time.sleep(args.poll)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\nCollector stopped.')
