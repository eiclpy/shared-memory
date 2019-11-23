# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
# Copyright (c) 2019 Huazhong University of Science and Technology, Dian Group
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation;
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Author: Pengyu Liu <eic_lpy@hust.edu.cn>

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
