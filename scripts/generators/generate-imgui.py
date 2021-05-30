#
# Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
# License: https://github.com/Rizz/sx#license-bsd-2-clause
#
# This file uses 'pycparser' to generate rizz Imgui bindings from preprocessed cimgui.h header file
# You must first preprocess the cimgui.h before submitting it to this program
# Here's clang command to preprocess the file first:
# Make sure to extract fake_libc_includes.zip to the current directory
#   1) clang -P -E -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS -Ifake_libc_include cimgui.h -D__int64="long long" -D"__declspec(dllexport)=" -Dextern="" > cimgui.i
#   2) python generate-imgui.py cimgui.i imgui-defs.h
#   3) Copy/Paste all the type parts (after CIMGUI_DEFINE_ENUMS_AND_STRUCTS in cimgui.h up to the API defs) to imgui.h
#   4) Now extract the API bindings and copy/paste them into imgui.h/imgui.c
#   5) change ImVec2 and ImVec4 typedefs:
#       typedef union sx_vec2 ImVec2;
#       typedef union sx_vec4 ImVec4;
#   6) in imgui.h, cimgui.h, change `#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS` to `#if !defined(CIMGUI_DEFINE_ENUMS_AND_STRUCTS) && !defined(RIZZ__IMGUI)` 
#   7) Change `.Render = imgui__render` in the api struct def
#   8) Remove all these references in cimgui.cpp and imgui.c:
#       ImGui::ShowDemoWindow
#       ImGui::ShowAboutWindow
#       ImGui::ShowStyleEditor
#       ImGui::ShowStyleSelector
#       ImGui::ShowFontSelector
#       ImGui::ShowUserGuide
#   
from __future__ import print_function
import sys
import os

# This is not required if you've installed pycparser into
# your site-packages/ with setup.py
sys.path.extend(['.', '..'])

from pycparser import c_parser, c_ast, parse_file, c_generator
import inspect

func_nodes = []
struct_nodes = []
typedef_nodes = []
enum_nodes = []

ignore_types = ['ImVec4', 'ImVec2', 'ImVec2_Simple', 'ImVec4_Simple', 'ImColor_Simple']
ignore_funcs_with_ret_types = ['ImVec4', 'ImVec2', 'ImVec2_Simple', 'ImVec4_Simple']

# A simple visitor for FuncDef nodes that prints the names and
# locations of function definitions.
class FuncCollector(c_ast.NodeVisitor):
    def __init__(self, _prefix, strip_prefix = True, comment = ''):
        self.prefix = _prefix
        self.prefix_len = len(_prefix)
        self.strip_prefix = strip_prefix
        self.comment = comment
    def visit_Decl(self, node):
        if (node.name is not None):
            if node.name[:self.prefix_len] == self.prefix:
                if (hasattr(node, 'type') and hasattr(node.type, 'type') and \
                    hasattr(node.type.type, 'type') and hasattr(node.type.type.type, 'names') and \
                    (node.type.type.type.names[0] in ignore_funcs_with_ret_types)):
                        return
                src_name = node.name
                if (self.strip_prefix):
                    node.name = node.name[self.prefix_len:] 
                f = {'node':node, 'src_name':src_name}
                if (self.comment):
                    f['comment'] = self.comment
                    self.comment = ''
                func_nodes.append(f)

class StructCollector(c_ast.NodeVisitor):
    def __init__(self, prefix):
        self.prefix = prefix
        self.prefix_len = len(prefix)

    def visit_Struct(self, node):
        if (node.name):
            if node.name[:self.prefix_len] == self.prefix:
                if not node.name in ignore_types:     struct_nodes.append(node)

class TypedefCollector(c_ast.NodeVisitor):
    def __init__(self, prefix):
        self.prefix = prefix
        self.prefix_len = len(prefix)

    def visit_Typedef(self, node):
        if (node.name):
            if node.name[:self.prefix_len] == self.prefix:
                if not node.name in ignore_types:     typedef_nodes.append(node)                    


class EnumCollector(c_ast.NodeVisitor):
    def __init__(self, prefix):
        self.prefix = prefix
        self.prefix_len = len(prefix)

    def visit_Enum(self, node):
        if (node.name):
            if node.name[:self.prefix_len] == self.prefix:
                if not node.name in ignore_types:     enum_nodes.append(node)                    


def generate_imgui(source_file, output_file):
    # Note that cpp is used. Provide a path to your own cpp or
    # make sure one exists in PATH.
    ast = parse_file(source_file)

    v = FuncCollector('ig')
    v.visit(ast)

    v = FuncCollector('ImGuiIO_', False, 'ImGuiIO')
    v.visit(ast)

    v = FuncCollector('ImDrawList_', False, 'ImDrawList')
    v.visit(ast)

    v = FuncCollector('ImFont_', False, 'ImDrawList')
    v.visit(ast)

    v = FuncCollector('ImFontAtlas_', False, 'ImFontAtlas')
    v.visit(ast)

    v = FuncCollector('ImGuiPayload_', False, 'ImGuiPayload')
    v.visit(ast)

    v = FuncCollector('ImGuiListClipper_', False, 'ImGuiListClipper')
    v.visit(ast)

    v = FuncCollector('ImGuiTextFilter_', False, 'ImGuiTextFilter')
    v.visit(ast)

    v = FuncCollector('ImGuiTextBuffer_', False, 'ImGuiTextBuffer')
    v.visit(ast)

    generator = c_generator.CGenerator()
    with open(output_file, 'wt') as file:

        file.write('typedef struct rizz_api_imgui\n{\n')
        for f in func_nodes:
            if ('comment' in f):
                file.write('    // ' + f['comment'] + '\n')
            node = f['node']
            src_name = f['src_name']
            line = generator.visit(node)
            name_idx = line.find(src_name)

            first_paran = line.find('(')

            # fix pointer star position
            first_star = line.find('*')
            if (first_star > 0 and line[first_star-1] == ' ' and first_star < first_paran):
                line = line[:first_star-1] + line[first_star:]
            if (first_star > 0 and (first_star+1) == name_idx and first_star < first_paran):
                line = line[:first_star] + ' ' + line[first_star:]                

            #
            first_space = -1
            space_idx = line.find(' ')
            while space_idx != -1:
                if (space_idx == name_idx-1):
                    first_space = space_idx
                    break
                space_idx = line.find(' ', space_idx+1)

            if (first_space != -1 and first_paran != -1):
                final_line = '    %-14s %s%s)%s;\n' % (line[:first_space+1], '(*', node.name, line[first_paran:])
                file.write(final_line)

        file.write('} rizz_api_imgui;\n')

        file.write('\nrizz_api_imgui the__imgui = {\n')
        for i, f in enumerate(func_nodes):
            node = f['node']
            src_name = f['src_name']
            line = '    .%s = %s' % (node.name, src_name)
            if i < len(func_nodes)-1:
                line = line + ',\n'
            else:
                line = line + '\n'
            file.write(line)
        file.write('};\n')
        file.close()

if __name__ == "__main__":
    if len(sys.argv) == 3:
        generate_imgui(sys.argv[1], sys.argv[2])
    else:
        print('invalid arguments: generate-imgui.py <preprocessed_file> <output_file>')
