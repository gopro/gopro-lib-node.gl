import os
import sys
import subprocess
import tempfile
import shlex


def _ipc_exec(*args):
    cmd = ('ngl-ipc',) + args
    print('+ ' + shlex.join(cmd))
    return subprocess.check_output(cmd, text=True)


def get_session_info(session_id):
    host, port = session_id.rsplit('-', maxsplit=1)
    output = _ipc_exec('-x', host, '-p', port, '-?')
    return dict(line.split('=', maxsplit=1) for line in output.splitlines())


def get_sessions():
    sessions = []
    session_dirs = []
    tmp_dir = os.path.join(tempfile.gettempdir(), 'ngl-desktop')
    if os.path.isdir(tmp_dir):
        session_dirs = os.listdir(tmp_dir)
    for session_dir in session_dirs:
        session_file = os.path.join(tmp_dir, session_dir, 'session')
        if not os.path.isfile(session_file):
            continue
        sessions.append((session_dir, 'local ngl-desktop'))

    remote_sessions = os.getenv('NGL_DESKTOP_REMOTE_SESSIONS', '').split()
    for session_id in remote_sessions:
        info = get_session_info(session_id)
        sessions.append((session_id, 'remote ngl-desktop'))
    return sessions


def sync_file(session_id, ifile, ofile):
    host, port = session_id.rsplit('-', maxsplit=1)
    if host == 'localhost':
        return ifile
    return _ipc_exec('-x', host, '-p', port, '-u', f'{ofile}={ifile}').rstrip()


def scene_change(session_id, scenefile, duration, aspect_ratio, framerate, clear_color, samples):
    host, port = session_id.rsplit('-', maxsplit=1)
    _ipc_exec(
        '-x', host,
        '-p', port,
        '-f', scenefile,
        '-t', '%f' % duration,
        '-a', '%d/%d'% aspect_ratio,
        '-r', '%d/%d' % framerate,
        '-c', '%08X' % clear_color,
        '-m', '%d' % samples,
    )
