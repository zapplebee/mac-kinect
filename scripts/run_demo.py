#!/usr/bin/env python3
"""Run native depth, AVFoundation RGB, and the local demo; Ctrl-C stops all three."""
import argparse
import signal
import os
import subprocess
import sys
import time
from pathlib import Path


def main():
    def stop(_signal, _frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('build-native'))
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--rgb-device', default='Azure Kinect 4K Camera')
    parser.add_argument('--backend', choices=('metal','cpu'), default='metal', help='depth filter backend (Metal falls back to CPU)')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    build = args.build.resolve()
    binary = build / 'bin/live_depth'
    if not binary.is_file():
        parser.error(f'Build live_depth first: {binary}')
    output = build / 'live'
    output.mkdir(exist_ok=True)
    commands = [
        ('capture', [str(binary), str(output)]),
        ('rgb', ['ffmpeg', '-nostdin', '-hide_banner', '-f', 'avfoundation',
                 '-framerate', '30', '-video_size', '1280x720', '-pixel_format', 'uyvy422',
                 '-i', args.rgb_device + ':none', '-vf', 'fps=10', '-q:v', '3',
                 '-update', '1', '-atomic_writing', '1', '-y', str(output / 'rgb.jpg')]),
        ('server', [sys.executable, str(root / 'demo/server.py'), str(output), '--port', str(args.port)]),
    ]
    processes, logs = [], []
    try:
        for name, command in commands:
            log = (output / f'{name}.log').open('w')
            logs.append(log)
            env = dict(os.environ, KINECT_DEPTH_BACKEND=args.backend)
            processes.append((name, subprocess.Popen(command, stdout=log, stderr=log, env=env)))
        print(f'Demo: http://127.0.0.1:{args.port}\nLogs: {output}\nCtrl-C stops all capture processes.', flush=True)
        while True:
            for name, process in processes:
                if process.poll() is not None:
                    raise RuntimeError(f'{name} exited ({process.returncode}); see {output}/{name}.log')
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        for _, process in processes:
            if process.poll() is None:
                process.terminate()
        for _, process in processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for log in logs:
            log.close()


if __name__ == '__main__':
    main()
