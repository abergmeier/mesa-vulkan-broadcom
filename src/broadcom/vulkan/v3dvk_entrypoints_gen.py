# coding=utf-8
#
# Copyright © 2015, 2017 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

import argparse
import math
import os
import xml.etree.cElementTree as et

from collections import OrderedDict, namedtuple
from mako.template import Template

from v3dvk_extensions import VkVersion, MAX_API_VERSION, EXTENSIONS

# We generate a static hash table for entry point lookup
# (vkGetProcAddress). We use a linear congruential generator for our hash
# function and a power-of-two size table. The prime numbers are determined
# experimentally.

LAYERS = [
    'v3d42',
]

TEMPLATE_H = Template("""\
/* This file generated from ${filename}, don't edit directly. */

% for e in instance_entrypoints:
  % if e.alias:
    <% continue %>
  % endif
  % if e.guard is not None:
#ifdef ${e.guard}
  % endif
  ${e.return_type} ${e.prefixed_name('v3dvk')}(${e.decl_params()});
  % if e.guard is not None:
#endif // ${e.guard}
  % endif
% endfor

% for e in device_entrypoints:
  % if e.alias:
    <% continue %>
  % endif
  % if e.guard is not None:
#ifdef ${e.guard}
  % endif
  % for layer in LAYERS:
  ${e.return_type} ${e.prefixed_name(layer)}(${e.decl_params()});
  % endfor
  % if e.guard is not None:
#endif // ${e.guard}
  % endif
% endfor
""", output_encoding='utf-8')

TEMPLATE_C = Template(u"""\
/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* This file generated from ${filename}, don't edit directly. */
""", output_encoding='utf-8')

U32_MASK = 2**32 - 1

PRIME_FACTOR = 5024183
PRIME_STEP = 19

class StringIntMapEntry(object):
    def __init__(self, string, num):
        self.string = string
        self.num = num

        # Calculate the same hash value that we will calculate in C.
        h = 0
        for c in string:
            h = ((h * PRIME_FACTOR) + ord(c)) & U32_MASK
        self.hash = h

        self.offset = None

def round_to_pow2(x):
    return 2**int(math.ceil(math.log(x, 2)))

class StringIntMap(object):
    def __init__(self):
        self.baked = False
        self.strings = dict()

    def add_string(self, string, num):
        assert not self.baked
        assert string not in self.strings
        assert 0 <= num < 2**31
        self.strings[string] = StringIntMapEntry(string, num)

    def bake(self):
        self.sorted_strings = \
            sorted(self.strings.values(), key=lambda x: x.string)
        offset = 0
        for entry in self.sorted_strings:
            entry.offset = offset
            offset += len(entry.string) + 1

        # Save off some values that we'll need in C
        self.hash_size = round_to_pow2(len(self.strings) * 1.25)
        self.hash_mask = self.hash_size - 1
        self.prime_factor = PRIME_FACTOR
        self.prime_step = PRIME_STEP

        self.mapping = [-1] * self.hash_size
        self.collisions = [0] * 10
        for idx, s in enumerate(self.sorted_strings):
            level = 0
            h = s.hash
            while self.mapping[h & self.hash_mask] >= 0:
                h = h + PRIME_STEP
                level = level + 1
            self.collisions[min(level, 9)] += 1
            self.mapping[h & self.hash_mask] = idx

EntrypointParam = namedtuple('EntrypointParam', 'type name decl')

class EntrypointBase(object):
    def __init__(self, name):
        self.name = name
        self.alias = None
        self.guard = None
        self.enabled = False
        self.num = None
        # Extensions which require this entrypoint
        self.core_version = None
        self.extensions = []

class Entrypoint(EntrypointBase):
    def __init__(self, name, return_type, params, guard=None):
        super(Entrypoint, self).__init__(name)
        self.return_type = return_type
        self.params = params
        self.guard = guard

    def is_device_entrypoint(self):
        return self.params[0].type in ('VkDevice', 'VkCommandBuffer', 'VkQueue')

    def prefixed_name(self, prefix):
        assert self.name.startswith('vk')
        return prefix + '_' + self.name[2:]

    def decl_params(self):
        return ', '.join(p.decl for p in self.params)

    def call_params(self):
        return ', '.join(p.name for p in self.params)

class EntrypointAlias(EntrypointBase):
    def __init__(self, name, entrypoint):
        super(EntrypointAlias, self).__init__(name)
        self.alias = entrypoint

    def is_device_entrypoint(self):
        return self.alias.is_device_entrypoint()

    def prefixed_name(self, prefix):
        return self.alias.prefixed_name(prefix)

def get_entrypoints(doc, entrypoints_to_defines):
    """Extract the entry points from the registry."""
    entrypoints = OrderedDict()

    for command in doc.findall('./commands/command'):
        if 'alias' in command.attrib:
            alias = command.attrib['name']
            target = command.attrib['alias']
            entrypoints[alias] = EntrypointAlias(alias, entrypoints[target])
        else:
            name = command.find('./proto/name').text
            ret_type = command.find('./proto/type').text
            params = [EntrypointParam(
                type=p.find('./type').text,
                name=p.find('./name').text,
                decl=''.join(p.itertext())
            ) for p in command.findall('./param')]
            guard = entrypoints_to_defines.get(name)
            # They really need to be unique
            assert name not in entrypoints
            entrypoints[name] = Entrypoint(name, ret_type, params, guard)

    for feature in doc.findall('./feature'):
        assert feature.attrib['api'] == 'vulkan'
        version = VkVersion(feature.attrib['number'])
        if version > MAX_API_VERSION:
            continue

        for command in feature.findall('./require/command'):
            e = entrypoints[command.attrib['name']]
            e.enabled = True
            assert e.core_version is None
            e.core_version = version

    supported_exts = dict((ext.name, ext) for ext in EXTENSIONS)
    for extension in doc.findall('.extensions/extension'):
        ext_name = extension.attrib['name']
        if ext_name not in supported_exts:
            continue

        ext = supported_exts[ext_name]
        ext.type = extension.attrib['type']

        for command in extension.findall('./require/command'):
            e = entrypoints[command.attrib['name']]
            e.enabled = True
            assert e.core_version is None
            e.extensions.append(ext)

    return [e for e in entrypoints.values() if e.enabled]


def get_entrypoints_defines(doc):
    """Maps entry points to extension defines."""
    entrypoints_to_defines = {}

    platform_define = {}
    for platform in doc.findall('./platforms/platform'):
        name = platform.attrib['name']
        define = platform.attrib['protect']
        platform_define[name] = define

    for extension in doc.findall('./extensions/extension[@platform]'):
        platform = extension.attrib['platform']
        define = platform_define[platform]

        for entrypoint in extension.findall('./require/command'):
            fullname = entrypoint.attrib['name']
            entrypoints_to_defines[fullname] = define

    return entrypoints_to_defines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--outdir', help='Where to write the files.',
                        required=True)
    parser.add_argument('--xml',
                        help='Vulkan API XML file.',
                        required=True,
                        action='append',
                        dest='xml_files')
    args = parser.parse_args()

    entrypoints = []

    for filename in args.xml_files:
        doc = et.parse(filename)
        entrypoints += get_entrypoints(doc, get_entrypoints_defines(doc))

    # Manually add CreateDmaBufImageINTEL for which we don't have an extension
    # defined.
    entrypoints.append(Entrypoint('vkCreateDmaBufImageINTEL', 'VkResult', [
        EntrypointParam('VkDevice', 'device', 'VkDevice device'),
        EntrypointParam('VkDmaBufImageCreateInfo', 'pCreateInfo',
                        'const VkDmaBufImageCreateInfo* pCreateInfo'),
        EntrypointParam('VkAllocationCallbacks', 'pAllocator',
                        'const VkAllocationCallbacks* pAllocator'),
        EntrypointParam('VkDeviceMemory', 'pMem', 'VkDeviceMemory* pMem'),
        EntrypointParam('VkImage', 'pImage', 'VkImage* pImage')
    ]))

    device_entrypoints = []
    instance_entrypoints = []
    for e in entrypoints:
        if e.is_device_entrypoint():
            device_entrypoints.append(e)
        else:
            instance_entrypoints.append(e)

    device_strmap = StringIntMap()
    for num, e in enumerate(device_entrypoints):
        device_strmap.add_string(e.name, num)
        e.num = num
    device_strmap.bake()

    instance_strmap = StringIntMap()
    for num, e in enumerate(instance_entrypoints):
        instance_strmap.add_string(e.name, num)
        e.num = num
    instance_strmap.bake()

    # For outputting entrypoints.h we generate a v3dvk_EntryPoint() prototype
    # per entry point.
    try:
        with open(os.path.join(args.outdir, 'v3dvk_entrypoints.h'), 'wb') as f:
            f.write(TEMPLATE_H.render(instance_entrypoints=instance_entrypoints,
                                      device_entrypoints=device_entrypoints,
                                      LAYERS=LAYERS,
                                      filename=os.path.basename(__file__)))
        with open(os.path.join(args.outdir, 'v3dvk_entrypoints.c'), 'wb') as f:
            f.write(TEMPLATE_C.render(instance_entrypoints=instance_entrypoints,
                                      device_entrypoints=device_entrypoints,
                                      LAYERS=LAYERS,
                                      instance_strmap=instance_strmap,
                                      device_strmap=device_strmap,
                                      filename=os.path.basename(__file__)))
    except Exception:
        # In the event there's an error, this imports some helpers from mako
        # to print a useful stack trace and prints it, then exits with
        # status 1, if python is run with debug; otherwise it just raises
        # the exception
        if __debug__:
            import sys
            from mako import exceptions
            sys.stderr.write(exceptions.text_error_template().render() + '\n')
            sys.exit(1)
        raise


if __name__ == '__main__':
    main()
