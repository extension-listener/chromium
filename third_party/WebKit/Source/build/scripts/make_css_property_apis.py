#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import json5_generator
import template_expander
import make_style_builder

from collections import namedtuple, defaultdict

# Gets the classname for a given property.
def get_classname(property):
    if property['api_class'] is True:
        # This property had the generated_api_class flag set in CSSProperties.json5.
        return 'CSSPropertyAPI' + property['upper_camel_name']
    # This property has a specified class name.
    assert isinstance(property['api_class'], str), \
        ("api_class value for " + property['api_class'] + " should be None, True or a string")
    return property['api_class']


class CSSPropertyAPIWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, json5_file_path):
        super(CSSPropertyAPIWriter, self).__init__(json5_file_path)
        self._outputs = {
            'CSSPropertyDescriptor.cpp': self.generate_property_descriptor_cpp,
        }

        # Temporary map of API classname to list of propertyIDs that the API class is for.
        properties_for_class = defaultdict(list)
        # Temporary map of API classname to set of method names this API class implements
        api_methods_for_class = defaultdict(set)
        for property in self._properties.values():
            if property['api_class'] is None:
                continue
            classname = get_classname(property)
            api_methods_for_class[classname] = property['api_methods']
            properties_for_class[classname].append(property['property_id'])
            self._outputs[classname + '.h'] = self.generate_property_api_h_builder(classname, property['api_methods'])

        # Stores a list of classes with elements (index, classname, [propertyIDs, ..], [api_methods, ...]).
        self._api_classes = []

        ApiClass = namedtuple('ApiClass', ('index', 'classname', 'property_ids', 'api_methods'))
        for i, classname in enumerate(properties_for_class.keys()):
            self._api_classes.append(ApiClass(
                index=i + 1,
                classname=classname,
                property_ids=properties_for_class[classname],
                api_methods=api_methods_for_class[classname],
            ))

    @template_expander.use_jinja('CSSPropertyDescriptor.cpp.tmpl')
    def generate_property_descriptor_cpp(self):
        return {
            'api_classes': self._api_classes,
            'api_methods': self.json5_file.parameters['api_methods']['valid_values'],
        }

    # Provides a function object given the classname of the property.
    def generate_property_api_h_builder(self, api_classname, api_methods):
        @template_expander.use_jinja('CSSPropertyAPIFiles.h.tmpl')
        def generate_property_api_h():
            return {
                'api_classname': api_classname,
                'api_methods': api_methods,
            }
        return generate_property_api_h

if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIWriter).main()
