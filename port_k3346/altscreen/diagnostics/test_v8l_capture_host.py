#!/usr/bin/env python3
"""Synthetic test for read-only capture path: no car validation implied."""
import socket,subprocess,time,pathlib,os
exe='/mnt/data/v8l_work/v8l_source/diagnostics/v8l-dsitracer-capture-host'
for port,payload,needle in [(1798,b'DSIKombiSync screenFormat=1\nDSIKombiSync screenFormat=2\n',b'screenFormat=2'),(20991,b'\x00\xff\x11\x22GAUGE\x00\x00',b'GAUGE')]:
    for path in pathlib.Path('/tmp').glob('v8l-dsitracer-'+str(port)+'-*'): path.unlink(missing_ok=True)
    (pathlib.Path('/tmp')/('v8l-dsitracer-'+str(port)+'.pid')).unlink(missing_ok=True)
    srv=socket.socket();srv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    srv.bind(('127.0.0.1',port));srv.listen(2);srv.settimeout(6)
    proc=subprocess.Popen([exe,str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    try:
        cli,_=srv.accept();cli.sendall(payload);cli.close()
        time.sleep(.5)
        proc.terminate();proc.wait(timeout=6)
        files=list(pathlib.Path('/tmp').glob('v8l-dsitracer-'+str(port)+'-??.*'))
        assert any(needle in f.read_bytes() for f in files),(port,files)
        status=(pathlib.Path('/tmp')/('v8l-dsitracer-'+str(port)+'-status.log')).read_text()
        assert 'PROCESS_ENTRY' in status and 'LOCK_ACQUIRED' in status and 'CONNECTED' in status and 'STOP_CAPTURE' in status,status
        print('HOST_SOCKET_CAPTURE=PASS',port,len(payload))
    finally:
        if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
        srv.close()
