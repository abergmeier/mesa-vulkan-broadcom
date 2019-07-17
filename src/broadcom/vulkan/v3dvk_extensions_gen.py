COPYRIGHT = """\
/*
 * Copyright 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
"""

import argparse
import xml.etree.cElementTree as et

from mako.template import Template

from v3dvk_extensions import *

platform_defines = []

def _init_exts_from_xml(xml):
    """ Walk the Vulkan XML and fill out extra extension information. """

    xml = et.parse(xml)

    ext_name_map = {}
    for ext in EXTENSIONS:
        ext_name_map[ext.name] = ext

    for platform in xml.findall('./platforms/platform'):
        platform_defines.append(platform.attrib['protect'])

    for ext_elem in xml.findall('.extensions/extension'):
        ext_name = ext_elem.attrib['name']
        if ext_name not in ext_name_map:
            continue

        ext = ext_name_map[ext_name]
        ext.type = ext_elem.attrib['type']

_TEMPLATE_H = Template(COPYRIGHT + """

#ifndef V3DVK_EXTENSIONS_H
#define V3DVK_EXTENSIONS_H

#include <stdbool.h>

#define V3DVK_INSTANCE_EXTENSION_COUNT ${len(instance_extensions)}

struct v3dvk_instance_extension_table {
   union {
      bool extensions[V3DVK_INSTANCE_EXTENSION_COUNT];
      struct {
%for ext in instance_extensions:
         bool ${ext.name[3:]};
%endfor
      };
   };
};

#define V3DVK_DEVICE_EXTENSION_COUNT ${len(device_extensions)}

struct v3dvk_device_extension_table {
   union {
      bool extensions[V3DVK_DEVICE_EXTENSION_COUNT];
      struct {
%for ext in device_extensions:
        bool ${ext.name[3:]};
%endfor
      };
   };
};

#endif /* V3DVK_EXTENSIONS_H */
""")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--out-c', help='Output C file.')
    parser.add_argument('--out-h', help='Output H file.')
    parser.add_argument('--xml',
                        help='Vulkan API XML file.',
                        required=True,
                        action='append',
                        dest='xml_files')
    args = parser.parse_args()

    for filename in args.xml_files:
        _init_exts_from_xml(filename)

    for ext in EXTENSIONS:
        assert ext.type == 'instance' or ext.type == 'device'

    template_env = {
        'API_VERSIONS': API_VERSIONS,
        'MAX_API_VERSION': MAX_API_VERSION,
        'instance_extensions': [e for e in EXTENSIONS if e.type == 'instance'],
        'device_extensions': [e for e in EXTENSIONS if e.type == 'device'],
        'platform_defines': platform_defines,
    }

    if args.out_h:
        with open(args.out_h, 'w') as f:
            f.write(_TEMPLATE_H.render(**template_env))
