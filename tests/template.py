# -*- coding:utf-8 -*-
# auto generated
from __future__ import absolute_import, unicode_literals
import enum


class {{class_name}}(enum.Enum):
    """
    {{description}}
    """
{{#records}}
    {{enum|upper_camel}} = {{id}}{{#id|eq:1}}  # <-- one{{/id|eq:1}}{{/records}}
