import os
import platform

if platform.system() == 'Windows':
    ngl_dll_dirs = os.getenv('NGL_DLL_DIRECTORY')
    if ngl_dll_dirs:
        dll_dirs = ngl_dll_dirs.split(';')
        for dll_dir in dll_dirs:
            os.add_dll_directory(dll_dir)

from _pynodegl import *
