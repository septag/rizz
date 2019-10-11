# ios build tool
# sep.tagh@gmail.com - http://github.com/septag/rizz
# License: https://github.com/septag/rizz#license-bsd-2-clause
#
# There currently are 3 main commands:
# 'configure': This is usually the first command you have to run in order to prepare the directory 
#              structure and build files.
# Arguments: see --help
#
# VERSION HISTORY:
#       1.0.0
#

import sys
import os
import argparse
import shutil
import subprocess
import platform
import logging
import tempfile
import fnmatch
import json
import zipfile
import xml.etree.ElementTree as xmltree
import hashlib
import threading

def make_plist(plist_filepath, args):
    with open(plist_filepath, 'w', encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n')
        f.write('<plist version="1.0">\n')
        f.write('<dict>\n')
        f.write('   <key>CFBundleExecutable</key>\n')
        f.write('   <string>{}</string>\n'.format(args.name))
        f.write('   <key>CFBundleDisplayName</key>\n')
        f.write('   <string>{}</string>\n'.format(args.display_name))
        f.write('   <key>CFBundleName</key>\n')
        f.write('   <string>{}</string>\n'.format(args.name))
        f.write('   <key>CFBundleVersion</key>\n')
        f.write('   <string>{}</string>\n'.format(str(args.app_version)))
        f.write('   <key>CFBundleShortVersionString</key>\n')
        f.write('   <string>{}</string>\n'.format(args.app_version_name))
        f.write('   <key>CFBundleIdentifier</key>\n')
        f.write('   <string>{}</string>\n'.format(args.package))
        f.write('</dict>\n')
        f.write('</plist>\n')

def make_vscode_workspace(workspace_filepath, cmake_source_root, build_dir):
    workspace_dir = os.path.dirname(workspace_filepath)
    build_dir = os.path.abspath(build_dir)

    workspace = {
        'folders': [{
            'path': os.path.relpath(cmake_source_root, workspace_dir).replace('\\', '/'),
        }],
        'settings': {
            'cmake.buildDirectory': build_dir.replace('\\', '/'),
            'cmake.sourceDirectory': os.path.abspath(cmake_source_root).replace('\\', '/'),
            'cmake.configureOnOpen': False,
            'C_Cpp.intelliSenseCachePath': build_dir.replace('\\', '/')
        }
    }
    with open(workspace_filepath, 'w', encoding='utf-8') as f:
        json.dump(workspace, f, indent=2)

def build_ios(args):
    build_root = os.path.join(args.target_dir, 'build')
    xcode_root = os.path.join(args.target_dir, 'xcode')
    plist_filepath = os.path.join(args.target_dir, 'Info.plist')
    assets_root = os.path.join(args.target_dir, 'assets')

    if (not os.path.isdir(args.target_dir)): 
        os.makedirs(args.target_dir)
    if (not os.path.isdir(build_root)):
        os.makedirs(build_root)
    if (not os.path.isdir(xcode_root)):
        os.makedirs(xcode_root)
    if (not os.path.isdir(assets_root)):
        os.makedirs(assets_root)

    build_dir = os.path.join(build_root, args.ios_platform, args.build_type)
    if (not os.path.isdir(build_dir)):
        os.makedirs(build_dir)       
    xcode_dir = os.path.join(xcode_root, args.ios_platform)
    if (not os.path.isdir(xcode_dir)):
        os.makedirs(xcode_dir)
    
    if (args.command == 'configure'):
        if (not os.path.isfile(plist_filepath)):
            if (not args.plist):
                make_plist(plist_filepath, args)
            else:
                shutil.copyfile(args.plist, plist_filepath)

        print('-- plist file: %s' % plist_filepath)
        print('-- source: %s' % args.cmake_source)

        # configure with cmake
        rizz_root_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
        cmake_toolchain_file = os.path.join(rizz_root_dir, 'cmake', 'ios.toolchain.cmake')
        assert os.path.isfile(cmake_toolchain_file)

        cmd = [
            'cmake',
            '-DIOS_PLATFORM=' + args.ios_platform.upper(),
            '-DIOS_ARCH=' + args.ios_arch,
            '-DIOS_DEPLOYMENT_TARGET=' + args.ios_min_version,
            '-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=.',
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=.',
            '-DBUNDLE=1',
            '-DBUNDLE_TARGET=' + args.cmake_target,
            '-DBUNDLE_TARGET_NAME=' + args.name,
            '-DMACOSX_BUNDLE_ROOT_DIR=' + os.path.abspath(args.target_dir)
        ]

        if (not args.macos):
            cmd.append('-DCMAKE_TOOLCHAIN_FILE=' + cmake_toolchain_file)

        if (platform.system() == 'Darwin'):
            print('')
            print('configure (ninja): %s' % args.ios_platform)
            try:
                cmd_build = cmd
                cmd_build.extend(['-DCMAKE_BUILD_TYPE=' + args.build_type, '-DCMAKE_EXPORT_COMPILE_COMMANDS=1', '-GNinja'])
                logging.debug(' '.join(cmd_build))
                cmake_result = subprocess.check_output(cmd_build, cwd=build_dir)
            except subprocess.CalledProcessError as e:
                logging.error('cmake returned with error code: %d' % e.returncode)
                exit(-1)
            else:
                for l in cmake_result.decode('ascii').split('\n'):
                    logging.debug(l)

            vscode_workspace = os.path.join(build_root, args.name + '-' + args.ios_platform + '-' + args.build_type + '.code-workspace')
            make_vscode_workspace(vscode_workspace, args.cmake_source, build_dir)

            # xcode
            print('')
            print('configure (xcode): %s' % args.ios_platform)
            try:
                cmd_xcode = cmd
                cmd_xcode.extend(['-GXcode'])
                logging.debug(' '.join(cmd))
                cmake_result = subprocess.check_output(cmd_xcode, cwd=xcode_dir)
            except subprocess.CalledProcessError as e:
                logging.error('cmake returned with error code: %d' % e.returncode)
                exit(-1)
            else:
                for l in cmake_result.decode('ascii').split('\n'):
                    logging.debug(l)

        # copy dummy resource file(s) - icons and launch images
        if (not os.path.isdir(os.path.join(args.target_dir, 'images.xcassets'))):
            print('copying initial resource files to: %s' % args.target_dir.replace('\\', '/'))
            cur_dir = os.path.dirname(os.path.abspath(__file__))
            with zipfile.ZipFile(os.path.join(cur_dir, 'ios-res.zip'), 'r') as f:
                f.extractall(args.target_dir)
        

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description='rizz ios build tool v1.0.0')
    arg_parser.add_argument('command', help='build command', choices=['configure'])
    arg_parser.add_argument('--name', help='app/cmake target name', required=True)
    arg_parser.add_argument('--display-name', help='app display name (default is same as `name`)')
    arg_parser.add_argument('--cmake-source', help='cmake source directory')
    arg_parser.add_argument('--cmake-target', help='cmake project name (default is same as `name`)')
    arg_parser.add_argument('--cmake-args', help='additional cmake arguemnts', default='')
    arg_parser.add_argument('--build-type', help='build type', choices=['Debug', 'Release', 'RelWithDebInfo'], default='Debug')
    arg_parser.add_argument('--target-dir', help='target build directory', required=True)
    arg_parser.add_argument('--package', help='app package name')
    arg_parser.add_argument('--app-version', help='app incremental version number', type=int, default=1)
    arg_parser.add_argument('--app-version-name', help='app version name (example: 1.1.0)', default='1.0.0')
    arg_parser.add_argument('--ios-min-version', help='iOS minimum deployment version', default='8.0')
    arg_parser.add_argument('--ios-platform', help='iOS target platform', choices=['ios', 'simulator', 'simulator64', 'tvos', 'simulator_tvos'], default='ios')
    arg_parser.add_argument('--ios-arch', help='iOS target architectures')
    arg_parser.add_argument('--plist', help='custom info.plist file', default='')
    arg_parser.add_argument('--macos', help='configure for MacOS build instead of iOS', action='store_const', const=True)
    arg_parser.add_argument('--verbose', help='enable verbose output', action='store_const', const=True)
    arg_parser.add_argument('--version', action='version', version='1.0.0')

    args = arg_parser.parse_args(sys.argv[1:])

    if ((not args.cmake_source or not os.path.isdir(args.cmake_source)) and args.command == 'configure'):
        logging.error('--cmake-source directory is invalid: %s' % (args.cmake_source if args.cmake_source else ''))
        exit(-1)
    args.cmake_source = os.path.normpath(args.cmake_source)

    if (not args.package and not args.plist and args.command == 'configure'):
        logging.error('--package argument is not provided')
        exit(-1)

    if (args.plist and not os.path.isfile(args.plist)) and args.command == 'configure':
        logging.error('invalid plist file: %s' % args.plist)
        exit(-1)

    if (not args.cmake_target):
        args.cmake_target = args.name
    
    if (not args.display_name):
        args.display_name = args.name

    if (not args.ios_arch):
        if (args.ios_platform == 'ios'):
            args.ios_arch = 'arm64'
        elif (args.ios_platform == 'simulator'):
            args.ios_arch = 'i386'
        elif (args.ios_platform == 'simulator64'):
            args.ios_arch = 'x86_64'
        elif (args.ios_platform == 'tvos'):
            args.ios_arch = 'arm64'
        elif (args.ios_platform == 'simulator_tvos'):
            args.ios_arch = 'x86_64'

    args.target_dir = os.path.normpath(args.target_dir)

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

    build_ios(args)