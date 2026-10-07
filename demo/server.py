#!/usr/bin/env python3
"""Local experimental depth preview and PNG endpoint for OBS Browser Source."""
import argparse
import os
import struct
import time
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs

WIDTH, HEIGHT = 640, 576


def png(depth, near, far, cutoff):
    pixels = bytearray(WIDTH * HEIGHT)
    for i, (d,) in enumerate(struct.iter_unpack('<H', depth)):
        pixels[i] = (0 if d == 0 else (255 if d <= cutoff else 0)) if cutoff else (
            0 if d == 0 else max(0, min(255, round(255 * (far-d)/(far-near)))))
    def chunk(t, b):
        return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b))
    scan = b''.join(b'\0'+pixels[y*WIDTH:(y+1)*WIDTH] for y in range(HEIGHT))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',WIDTH,HEIGHT,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path)
    p.add_argument('--port',type=int,default=8765)
    args=p.parse_args()
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            url=urlparse(self.path)
            try:
                frame_time=None
                if url.path=='/': body,kind=Path(__file__).with_name('index.html').read_bytes(),'text/html'
                elif url.path=='/rgb.jpg':
                    path=args.directory/'rgb.jpg'
                    with path.open('rb') as file:
                        frame_time=os.fstat(file.fileno()).st_mtime
                        body=file.read()
                    if time.time()-frame_time>2: raise ValueError('RGB stream stale')
                    kind='image/jpeg'
                elif url.path in ('/frame.bin','/aligned.bin','/depth.png','/mask.png'):
                    path=args.directory/('aligned.bin' if url.path=='/aligned.bin' else 'frame.bin')
                    if time.time()-path.stat().st_mtime>2: raise ValueError('Depth stream stale; no current frame')
                    with path.open('rb') as file:
                        frame_time=os.fstat(file.fileno()).st_mtime
                        data=file.read()
                    pixels=1280*720 if url.path=='/aligned.bin' else WIDTH*HEIGHT
                    if len(data)!=40+pixels*2 or struct.unpack_from('<Q',data)[0]!=0x315448504544344b:
                        raise ValueError('Invalid depth frame')
                    if url.path in ('/frame.bin','/aligned.bin'): body,kind=data,'application/octet-stream'
                    else:
                        q=parse_qs(url.query);near=int(q.get('near',['500'])[0]);far=int(q.get('far',['4000'])[0]);cut=int(q.get('cutoff',['1500'])[0]) if url.path=='/mask.png' else 0
                        if not 0<=near<far<=65535 or not 0<=cut<=65535: raise ValueError('Invalid distance range')
                        body,kind=png(data[40:],near,far,cut),'image/png'
                else: self.send_error(404);return
                self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(body)))
                if frame_time is not None: self.send_header('X-Frame-Time',str(frame_time))
                self.end_headers();self.wfile.write(body)
            except (OSError,ValueError) as error:
                self.send_error(503,str(error))
    print(f'Experimental native depth: http://127.0.0.1:{args.port}',flush=True)
    ThreadingHTTPServer(('127.0.0.1',args.port),Handler).serve_forever()


if __name__=='__main__': main()
