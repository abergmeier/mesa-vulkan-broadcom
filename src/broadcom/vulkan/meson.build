# Copyright © 2019 Andreas Bergmeier
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

v3dvk_extensions_py = files('v3dvk_extensions.py')

v3dvk_entrypoints = custom_target(
  'v3dvk_entrypoints.[ch]',
  input : ['v3dvk_entrypoints_gen.py', vk_api_xml],
  output : ['v3dvk_entrypoints.h', 'v3dvk_entrypoints.c'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--outdir', meson.current_build_dir(),
  ],
  depend_files : v3dvk_extensions_py,
)

v3dvk_extensions_h = custom_target(
  'v3dvk_extensions.h',
  input : ['v3dvk_extensions_gen.py', vk_api_xml],
  output : 'v3dvk_extensions.h',
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--out-h', '@OUTPUT@',
  ],
  depend_files : v3dvk_extensions_py,
)

v3dvk_extensions_c = custom_target(
  'v3dvk_extensions.c',
  input : ['v3dvk_extensions_gen.py', vk_api_xml],
  output : 'v3dvk_extensions.c',
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@',
    '--out-c', '@OUTPUT@',
  ],
  depend_files : v3dvk_extensions_py,
)

broadcom_icd = custom_target(
  'broadcom_icd',
  input : 'v3dvk_icd.py',
  output : 'broadcom_icd.@0@.json'.format(host_machine.cpu()),
  command : [
    prog_python, '@INPUT@',
    '--lib-path', join_paths(get_option('prefix'), get_option('libdir')),
    '--out', '@OUTPUT@',
  ],
  depend_files : v3dvk_extensions_py,
  build_by_default : true,
  install_dir : with_vulkan_icd_dir,
  install : true,
)

dev_icd = custom_target(
  'dev_icd',
  input : 'v3dvk_icd.py',
  output : 'dev_icd.@0@.json'.format(host_machine.cpu()),
  command : [
    prog_python, '@INPUT@', '--lib-path', '/usr/local/lib',
    '--out', '@OUTPUT@'
  ],
  depend_files : files('v3dvk_extensions.py'),
  build_by_default : true,
  install : false,
)

libv3dvk_files = files(
  'common.h',
  'device.c',
  'device.h',
  'icd.c',
  'instance.c',
  'instance.h',
  'ioctl.h',
  'valgrind.h',
  'v3dvk_batch.c',
  'v3dvk_cmd_buffer.c',
  'v3dvk_cmd_pool.c',
  'v3dvk_formats.c',
  'v3dvk_gem.c',
  'v3dvk_physical_device.c',
  'v3dvk_queue.c',
  'v3dvk_macro.h',
  'v3dvk_semaphore.h',
  'v3dvk_util.c',
  'wsi.c',
)

v3dvk_deps = [
  dep_libdrm,
  dep_valgrind,
  idep_vulkan_util_headers,
]
v3dvk_flags = [
  c_vis_args,
  no_override_init_args,
]

libv3dvk_common = static_library(
  'v3dvk_common',
  [
    v3dvk_entrypoints, v3dvk_extensions_c, v3dvk_extensions_h,
    libv3dvk_files,
  ],
  include_directories : [
    inc_common, inc_broadcom, inc_include,
    inc_vulkan_wsi,
  ],
  c_args : v3dvk_flags,
  dependencies : v3dvk_deps,
)

libvulkan_broadcom = shared_library(
  'vulkan_broadcom',
  [v3dvk_extensions_h],
  include_directories : [
    inc_common, inc_broadcom, inc_common, inc_src,
  ],
  link_whole : [libv3dvk_common],
  link_with : [
    libcompiler, libbroadcom_cle, libbroadcom_v3d,
    libvulkan_wsi, libmesa_util,
  ],
  dependencies : [
    dep_thread, dep_dl, dep_m, v3dvk_deps, idep_vulkan_util,
  ],
  c_args : v3dvk_flags,
  link_args : ['-Wl,--build-id=sha1', ld_args_bsymbolic, ld_args_gc_sections],
  install : true,
)
