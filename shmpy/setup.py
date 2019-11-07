try:
    import setuptools as distutools
except ImportError:
    import distutils.core as distutools

name = "shm_pool"
description = "Shm interface for Python"
long_description = open("README.md", "r").read()
author = "Pengyu Liu"
author_email = "eic_lpy@hust.edu.cn"

extension = distutools.Extension("shm_pool",
                                 ["memory-pool.c", "memory-pool-module.c"],
                                 # extra_compile_args=['-E'],
                                 depends=[
                                     "memory-pool.c",
                                     "memory-pool.h",
                                     "memory-pool-module.c",
                                     "setup.py",
                                 ],
                                 )

distutools.setup(name=name,
                 version='0.0.1',
                 description=description,
                 long_description=long_description,
                 author=author,
                 author_email=author_email,
                 py_modules=['py_interface'],
                 ext_modules=[extension]
                 )
