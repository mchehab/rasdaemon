#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 2026: Mauro Carvalho Chehab <mchehab@kernel.org>.

import re

from kdoc.kdoc_re import KernRe
from kdoc.c_lex import CMatch, CTokenizer

struct_args_pattern = r"([^,)]+)"


class CTransforms:
    """
    Data class containing a long set of transformations to turn
    structure member prefixes, and macro invocations and variables
    into something we can parse and generate kdoc for.
    """

    #: Transforms for structs and unions.
    struct_xforms = []

    #: Transforms for function prototypes.
    function_xforms = []

    #: Transforms for variable prototypes.
    var_xforms = []

    #: Transforms main dictionary used at apply_transforms().
    xforms = {
        "struct": struct_xforms,
        "func": function_xforms,
        "var": var_xforms,
    }

    def apply(self, xforms_type, source):
        """
        Apply a set of transforms to a block of source.

        As tokenizer is used here, this function also remove comments
        at the end.
        """
        if xforms_type not in self.xforms:
            return source

        if isinstance(source, str):
            source = CTokenizer(source)

        for search, subst in self.xforms[xforms_type]:
            #
            # KernRe only accept strings.
            #
            if isinstance(search, KernRe):
                source = str(source)

            source = search.sub(subst, source)
        return str(source)
