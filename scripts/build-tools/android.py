# android build tool
# sep.tagh@gmail.com - http://github.com/septag/rizz
# License: https://github.com/septag/rizz#license-bsd-2-clause
#
# There currently are 3 main commands:
# 'configure': This is usually the first command you have to run in order to prepare the directory 
#              structure and build files.
# 'build': Builds the cpp and java projects and moves the generated .so file to their proper ABI folder 
# 'package': Packages the files and builds the APK and sign it. 
#
# Arguments: see --help
# Examples:
# 1) prepare the '02-quad' example in `android/quad` directory, the package will be named 'com.glitterbombg.quad':
#    ```python android.py --package com.glitterbombg.quad --name quad --cmake-target 02-quad --sdk-root=/path/to/android/sdk --target-dir=android/quad --cmake-source=/path/to/rizz configure```
#    after successfully finished, you can browse the `android/quad` directory for generated files:
#       - `apk` directory will contain native libraries, compiled java and assets. you can sync or copy 
#         your assets into this folder
#       - `build` contains intermediate build files and generated APK files.
#         there is also .code-workspace file for vscode users that you can open and build the ndk code 
#         with vscode. You will need microsoft C/C++ extension and CMake tools extension for this purpose
#       - `src` contains java code, you can add any extra java code here
#       - `res` will be populated with minimal resources and icons for the program
#       - `AndroidManifest.xml` will be generated if you don't provide one, you can edit this file for your purposes
#         But note that some arguments like `--sdk-min-platform-version`, `--sdk-platform-version` and `--app-version` will affect this file
# 2) Build the binaries and java dex files
#    ```python android.py --package com.glitterbombg.quad --name quad --cmake-target 02-quad --sdk-root=/path/to/android/sdk --target-dir=android/quad --cmake-source=/path/to/rizz build```
# 3) Make the final APK
#    ```python android.py --package com.glitterbombg.quad --name quad --cmake-target 02-quad --sdk-root=d:/sdk/android --target-dir=android/quad --cmake-source=c:/projects/rizz --copy=/your/second/location package```
#    Makes the final APK file, signs it (with debug key) and copy it to another location
#    if you provide `--keystore`, `--key-alias`, `--keystore-password`, `--key-password` then it will signed by your custom key
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
import queue
import time

if (platform.system() == 'Windows'):
    EXE = '.exe'
    BAT = '.bat'
else:
    EXE = ''
    BAT = ''

def find_cmake_latest(args):
    # check in sdk directory
    cmake_root_dir = os.path.join(args.sdk_root, 'cmake')
    dirs = [f for f in os.listdir(cmake_root_dir) if os.path.isdir(os.path.join(cmake_root_dir, f))]
    latest_dir = None
    for d in dirs:
        if latest_dir:
            latest_dir_version = latest_dir.split('.')
            versions = d.split('.')
            if (int(versions[0]) > int(latest_dir_version[0]) and 
                int(versions[1]) > int(latest_dir_version[1]) and
                int(versions[2]) > int(latest_dir_version[2])):
                latest_dir = d
        else:
            latest_dir = d
        
    return os.path.join(cmake_root_dir, latest_dir) if latest_dir else None

def setup_manifest(manifest_filepath, args):
    xmlroot = xmltree.parse(manifest_filepath).getroot()
    if (xmlroot.tag == 'manifest'):
        # modify sdk version
        xmlsdk = xmlroot.find('uses-sdk')
        for key in xmlsdk.attrib:
            if key.endswith('minSdkVersion'):
                xmlsdk.attrib[key] = str(args.sdk_min_platform_version)
            if key.endswith('targetSdkVersion'):
                xmlsdk.attrib[key] = str(args.sdk_platform_version)

        # modify app version
        for key in xmlroot.attrib:
            if (key.endswith('versionCode')):
                xmlroot.attrib[key] = str(args.app_version)
            if (key.endswith('versionName')):
                xmlroot.attrib[key] = str(args.app_version_name)

        # modify application 
        xmlapp = xmlroot.find('application')
        for key in xmlapp.attrib:
            if key.endswith('debuggable'):
                xmlapp.attrib[key] = 'true' if args.build_type == 'Debug' or args.build_type == 'RelWithDebInfo' else 'false'

    newxml = xmltree.ElementTree(xmlroot)
    newxml.write(manifest_filepath)

def adb_check_file_exists(ADB, path_on_device, run_as = None):
    try:
        if (run_as):
            cmd = [ADB, 'shell', 'run-as', run_as, 'ls ' + path_on_device]
        else:
            cmd = [ADB, 'shell', 'ls ' + path_on_device]
        logging.debug(' '.join(cmd))
        adb_result = subprocess.check_output(cmd)
    except subprocess.CalledProcessError as e:
        logging.error("adb returned with error code: %d" % e.returncode)
        exit(-1)
    else:
        return True if adb_result.decode('ascii').strip() == path_on_device else False

def adb_get_abi(ADB):
    try:
        cmd = [
            ADB, 'shell',
            'getprop', 'ro.product.cpu.abi'
        ]
        logging.debug(' '.join(cmd))
        adb_result = subprocess.check_output(cmd)
    except subprocess.CalledProcessError as e:
        logging.error("adb returned with error code: %d" % e.returncode)
        exit(-1)
    else:
        return adb_result.decode('ascii').strip()

def adb_upload_debugger(ADB, ndk_root, package_name, debugger_name):
    if not adb_check_file_exists(ADB, debugger_name, run_as=package_name):
        abi = adb_get_abi(ADB)
        abi_to_gdb = { 
            'armeabi-v7a': 'android-arm', 
            'arm64-v8a': 'android-arm64',
            'x86': 'android-x86',
            'x86_64': 'android-x86_64'}
    
        local_debugger_path = os.path.join(ndk_root, 'prebuilt', abi_to_gdb[abi], 'gdbserver', 'gdbserver')
        if os.path.isfile(local_debugger_path):
            print('found {}: {}'.format(debugger_name, local_debugger_path))
        else:
            logging.error('debugger not found: {}'.format(debugger_name))
            return False
    
        remote_debugger_path = '/data/data/{}/{}'.format(package_name, debugger_name)
        remote_debugger_path_tmp = '/data/local/tmp/' + debugger_name
        if not adb_check_file_exists(ADB, remote_debugger_path_tmp):
            cmd = [
                ADB, 'push',
                local_debugger_path, 
                '/data/local/tmp'
            ]
            logging.debug(' '.join(cmd))
            if subprocess.call(cmd) != 0:
                return False
            cmd = [
                ADB, 'shell',
                'chmod', '+x',
                remote_debugger_path_tmp
            ]
            if subprocess.call(cmd) != 0:
                return False

            print('uploaded {} debugger to /data/local/tmp'.format(debugger_name))
        
        # copy from /tmp to data directory of the program
        cmd = [
            ADB, 'shell',
            'run-as', package_name,
            'cp', remote_debugger_path_tmp, remote_debugger_path
        ]
        logging.debug(' '.join(cmd))
        if subprocess.call(cmd) != 0:
            return False
        print('copied {} debugger to programs data path: {}'.format(debugger_name, os.path.dirname(remote_debugger_path)))
    
    return True

def adb_run_debugger(ADB, JDB, package_name, activity_name, debugger_name, port):
    # run program and wait for the debugger
    cmd = [
        ADB, 'shell',
        'am', 'start',
        '-D', '-n',
        '"{}/{}"'.format(package_name, activity_name)
    ]    
    logging.debug(' '.join(cmd))
    if subprocess.call(cmd):
        return False
    
    cmd = [
        ADB, 'shell',
        'run-as', package_name,
        './' + debugger_name
    ]
    return True

def adb_get_process_id(ADB, package_name):
    try:
        cmd = [
            ADB, 'shell',
            'pgrep', package_name
        ]
        logging.debug(' '.join(cmd))
        adb_result = subprocess.check_output(cmd)
    except subprocess.CalledProcessError as e:
        logging.error("adb returned with error code: %d" % e.returncode)
        exit(-1)
    else:
        return int(adb_result.decode('ascii').strip())

def get_activity_name(ADB, package_name, manifest_filepath):
    xmlroot = xmltree.parse(manifest_filepath).getroot()
    xmlapp = xmlroot.find('application')
    assert xmlapp
    for appchild in xmlapp:
        if appchild.tag == 'activity':
            xmlintent = appchild.find('intent-filter')
            activity_name = ''
            for key in appchild.attrib:
                if key.endswith('name'):
                    activity_name = appchild.attrib[key] 
            if xmlintent:
                for intentchild in xmlintent:
                    if intentchild.tag == 'category':
                        for key in intentchild.attrib:
                            if key.endswith('name') and intentchild.attrib[key].endswith('LAUNCHER'):
                                return activity_name
    return None

def make_manifest(manifest_filepath, args):
    with open(manifest_filepath, 'w') as f:
        debuggable = 'true' if args.build_type == 'Debug' or args.build_type == 'RelWithDebInfo' else 'false'
        f.write('<manifest xmlns:android="http://schemas.android.com/apk/res/android"\n')
        f.write('  package="{}"\n'.format(args.package))
        f.write('  android:versionCode="1"\n')
        f.write('  android:versionName="1.0">\n')
        f.write('  <uses-sdk android:minSdkVersion="{}" android:targetSdkVersion="{}"/>\n'.format(args.sdk_min_platform_version, args.sdk_platform_version))
        f.write('  <uses-permission android:name="android.permission.INTERNET"></uses-permission>\n')
        f.write('  <uses-feature android:glEsVersion="0x00030000"></uses-feature>\n')
        f.write('  <application android:label="{}" android:debuggable="{}" android:hasCode="false">\n'.format(args.name, debuggable))
        f.write('    <activity android:name="android.app.NativeActivity"\n')
        f.write('      android:label="{}"\n'.format(args.name))
        f.write('      android:launchMode="singleTask"\n')
        f.write('      android:screenOrientation="fullUser"\n')
        f.write('      android:configChanges="orientation|screenSize|keyboard|keyboardHidden">\n')
        f.write('      <meta-data android:name="android.app.lib_name" android:value="{}"/>\n'.format(args.name))
        f.write('      <intent-filter>\n')
        f.write('        <action android:name="android.intent.action.MAIN"/>\n')
        f.write('        <category android:name="android.intent.category.LAUNCHER"/>\n')
        f.write('      </intent-filter>\n')
        f.write('    </activity>\n')
        f.write('  </application>\n')
        f.write('</manifest>\n')
        f.close()

def java_get_rt(args):
    tmp_dir = tempfile.gettempdir()
    tmp_filename_class = os.path.join(tmp_dir, 'GetRT.class') 

    if (not os.path.isfile(tmp_filename_class)):
        tmp_filename_java = os.path.join(tmp_dir, 'GetRT.java')

        with open(tmp_filename_java, 'w') as f:
            f.write('public class GetRT {\n')
            f.write('\tpublic static void main(String[] args) throws Exception {\n')
            f.write('\t\tSystem.out.println(System.getProperty("sun.boot.class.path"));\n')
            f.write('\t}\n')
            f.write('}\n')
            f.close()

        javac_exe = os.path.join(args.java_root, 'bin', 'javac' + EXE).replace('\\', '/')
        result = subprocess.call([javac_exe, 'GetRT.java'], cwd=tmp_dir)
        if (result != 0):
            logging.error('Compile intermediate file GetRT.java failed')
            exit(-1)
    
    java_exe = os.path.join(args.java_root, 'bin/java.exe').replace('\\', '/')
    jre_paths = subprocess.check_output([java_exe, 'GetRT'], cwd=tmp_dir).decode('utf-8')

    if (platform.system() == 'Windows'):
        jre_paths = jre_paths.replace('\\', '/').split(';')
    else:
        jre_paths = jre_paths.split(':')

    for jre_path in jre_paths:
        if jre_path.endswith('rt.jar'):
            return os.path.normpath(jre_path)

    return ''

def make_vscode_workspace(workspace_filepath, java_src_root, cmake_source_root, cmake_exe, build_dir, abi, args):
    workspace_dir = os.path.dirname(workspace_filepath)
    build_dir = os.path.abspath(build_dir)

    workspace = {
        'folders': [
            {   
                'name': 'java',
                'path': os.path.relpath(java_src_root, workspace_dir).replace('\\', '/')
            },
            {
                'name': 'native',
                'path': os.path.relpath(cmake_source_root, workspace_dir).replace('\\', '/')
            }
        ],
        'settings': {
            'cmake.buildDirectory': build_dir.replace('\\', '/'),
            'cmake.sourceDirectory': os.path.abspath(cmake_source_root).replace('\\', '/'),
            'cmake.cmakePath': cmake_exe.replace('\\', '/'),
            'cmake.configureOnOpen': False,
            'C_Cpp.intelliSenseCachePath': build_dir.replace('\\', '/')
        }
    }
    with open(workspace_filepath, 'w', encoding='utf-8') as f:
        json.dump(workspace, f, indent=2)

    # check source folders .vscode directory and add cpp-tools configuration data for android builds
    vscode_cpp_config_filepath = os.path.join(cmake_source_root, '.vscode')
    if not os.path.isdir(vscode_cpp_config_filepath):
        os.makedirs(vscode_cpp_config_filepath)
    vscode_cpp_config_filepath = os.path.join(vscode_cpp_config_filepath, 'c_cpp_properties.json')
    if os.path.isfile(vscode_cpp_config_filepath):
        with open(vscode_cpp_config_filepath, 'r') as f:
            cpp_config_data = json.load(f)
    else:
        cpp_config_data = {'configurations': [], 'version': 4}

    android_conf_exists = False
    for conf in cpp_config_data['configurations']:
        if conf['name'] == 'Android' + '-' + abi:
            android_conf_exists = True
            break

    if not android_conf_exists:
        compiler_prefix_dir = os.path.join(args.ndk_root, 'toolchains', 'llvm', 'prebuilt',  
            str.lower(platform.system() + '-x86_64'))
        if (abi == 'armeabi-v7a'):
            compiler_target = 'armv7-none-linux-androideabi'
        if (abi == 'arm64-v8a'):
            compiler_target = 'aarch64-none-linux-android'
        if (abi == 'x86'):
            compiler_target = 'i686-none-linux-android'
        if (abi == 'x86_64'):
            compiler_target = 'x86_64-none-linux-android'

        compiler_path = os.path.join(compiler_prefix_dir, 'bin', 'clang' + EXE)
        compiler_path = ' '.join([compiler_path, '--gcc-toolchain=' + compiler_prefix_dir, 
            '--sysroot=' + os.path.join(compiler_prefix_dir, 'sysroot'), '--target=' + compiler_target])
        android_conf = {
            'name': 'Android' + '-' + abi,
            'cppStandard': 'c++11',
            'cStandard': 'c11',
            'intelliSenseMode': 'clang-x64',
            'compilerPath': compiler_path.replace('\\', '/'),
            'compileCommands': os.path.join(build_dir,  'compile_commands.json').replace('\\', '/')
        }

        cpp_config_data['configurations'].append(android_conf)
        with open(vscode_cpp_config_filepath, 'w') as f:
            json.dump(cpp_config_data, f, indent=4)

    # check java folders .vscode directory and add luanch information for android debugging
    # needs android for vscode extension: https://marketplace.visualstudio.com/items?itemName=adelphes.android-dev-ext
    vscode_java_launch_filepath = os.path.join(java_src_root, '.vscode')
    if not os.path.isdir(vscode_java_launch_filepath):
        os.makedirs(vscode_java_launch_filepath)
    vscode_java_launch_filepath = os.path.join(vscode_java_launch_filepath, 'launch.json')
    if os.path.isfile(vscode_java_launch_filepath):
        with open(vscode_java_launch_filepath, 'r') as f:
            java_launch_data = json.load(f)
    else:
        java_launch_data = {'version': '0.2.0', 'configurations': []}

    apk_launch_exists = False
    for conf in java_launch_data['configurations']:
        if 'type' in conf and conf['type'] == 'android':
            apk_launch_exists = True
            break
    
    if not apk_launch_exists:
        apk_path = os.path.join(args.target_dir, 'build', args.name + (('-' + args.app_version_name) if args.app_version_name else '') + '.apk')
        apk_launch_data = {
            'type': 'android',
            'request': 'launch',
            'name': args.name,
            'appSrcRoot': os.path.abspath(args.target_dir).replace('\\', '/'),
            'apkFile': os.path.abspath(apk_path).replace('\\', '/'),
            'adbPort': 5037
        }
        java_launch_data['configurations'].append(apk_launch_data)
        with open(vscode_java_launch_filepath, 'w') as f:
            json.dump(java_launch_data, f, indent=4)

def java_compile_sources(args, java_class_root, java_src_root, apk_root, RUNTIME_JAR, ANDROID_JAR, DEX):
    javac_exe = os.path.join(args.java_root, 'bin', 'javac' + EXE).replace('\\', '/')
    print('')
    print('compiling java sources ...')
    for root, dirnames, filenames in os.walk(java_src_root):
        for filename in fnmatch.filter(filenames, '*.java'):
            print(filename)
            # compile java sources
            try:
                cmd = [
                    javac_exe, 
                    '-d', java_class_root, 
                    '-source', '1.7',
                    '-target', '1.7',
                    '-sourcepath', 'src',
                    '-bootclasspath', RUNTIME_JAR,
                    '-classpath', ANDROID_JAR,
                    os.path.join(root, filename)
                ]
                logging.debug(' '.join(cmd))
                javac_result = subprocess.check_output(cmd)
            except subprocess.CalledProcessError as e:
                logging.error("aapt returned with error code: %d" % e.returncode)
                exit(-1)
            else:
                for l in javac_result.decode('ascii').split('\n'):
                    logging.debug(l)        

    # build dex from classes
    print('')
    print('building DEX file ...')
    try:
        cmd = [
            DEX,
            '--verbose',
            '--dex',
            '--output=' + os.path.join(apk_root, 'classes.dex'),
            java_class_root
        ]
        logging.debug(' '.join(cmd))
        dex_result = subprocess.check_output(cmd)
    except subprocess.CalledProcessError as e:
        logging.error("aapt returned with error code: %d" % e.returncode)
        exit(-1)
    else:
        for l in dex_result.decode('ascii').split('\n'):
            logging.debug(l)

class AsyncFileReader(threading.Thread):
    def __init__(self, fd, process_id):
        assert callable(fd.readline)
        threading.Thread.__init__(self)
        self._fd = fd
        self._stopped = False
        self._process_id = str(process_id)
    
    def run(self):
        for line in iter(self._fd.readline, ''):
            line = line.decode('utf-8').strip()
            proc_id = line[line.find('(')+1:line.find(')')].strip()
            if (proc_id == self._process_id):
                print(line)
    
    def eof(self):
        return not self.is_alive()

    def stop(self):
        self._stopped = True
    
def build_android(args):
    if (not 'JAVA_HOME' in os.environ):
        os.environ['JAVA_HOME'] = args.java_root

    BUILD_TOOLS = os.path.join(args.sdk_root, 'build-tools', args.sdk_buildtools_version)
    if (not os.path.isdir(BUILD_TOOLS)):
        logging.error('invalid build-tools path: %s' % (BUILD_TOOLS))
        exit(-1)
    PLATFORM_TOOLS = os.path.join(args.sdk_root, 'platform-tools')
    if (not os.path.isdir(PLATFORM_TOOLS)):
        logging.error('invalid platform-tools path: %s' % (PLATFORM_TOOLS))
        exit(-1)

    AAPT = os.path.join(BUILD_TOOLS, 'aapt' + EXE)
    DEX = os.path.join(BUILD_TOOLS, 'dx' + BAT)
    ZIPALIGN = os.path.join(BUILD_TOOLS, 'zipalign' + EXE)
    APKSIGNER = os.path.join(BUILD_TOOLS, 'apksigner' + BAT)
    KEYTOOL = os.path.join(args.java_root, 'bin', 'keytool' + EXE)
    RUNTIME_JAR = java_get_rt(args)
    ANDROID_JAR = os.path.join(args.sdk_root, 'platforms', 'android-' + str(args.sdk_platform_version), 'android.jar')
    ADB = os.path.join(PLATFORM_TOOLS, 'adb' + EXE)

    # make intermediate directories and build the 'so' files
    build_root = os.path.join(args.target_dir, 'build')
    apk_root = os.path.join(args.target_dir, 'apk')
    libs_root = os.path.join(apk_root, 'lib')
    src_root = os.path.join(args.target_dir, 'src')
    res_root = os.path.join(args.target_dir, 'res')
    java_class_root = os.path.join(build_root, 'java')
    manifest_filepath = os.path.join(args.target_dir, 'AndroidManifest.xml')
    
    if (not os.path.isdir(args.target_dir)): 
        os.makedirs(args.target_dir)
    if (not os.path.isdir(src_root)):
        os.makedirs(src_root)
    if (not os.path.isdir(build_root)):
        os.makedirs(build_root)
    if (not os.path.isdir(apk_root)):
        os.makedirs(apk_root)
    if (not os.path.isdir(libs_root)):
        os.makedirs(libs_root)
    if (not os.path.isdir(java_class_root)):
        os.makedirs(java_class_root)

    cmake_root_dir = find_cmake_latest(args)
    if (not cmake_root_dir):
        args.make_path = os.path.join(args.ndk_root, 'prebuilt', 
                                      str.lower(platform.system() + '-x86_64'), 'bin', 'make'+EXE)
        args.cmake_path = 'cmake' + EXE
    else:
        args.ninja_path = os.path.join(cmake_root_dir, 'bin', 'ninja'+EXE)
        args.cmake_path = os.path.join(cmake_root_dir, 'bin', 'cmake'+EXE)
    
    if (args.command == 'configure'):
        print('-- source: %s' % args.cmake_source)
        print('-- cmake: %s' % args.cmake_path)
        print('-- make: %s' % args.ninja_path if args.ninja_path else args.make_path)
        
    print('-- aapt: %s' % AAPT)
    print('-- dx: %s' % DEX)
    print('-- adb: %s' % ADB)
    print('-- zipalign: %s' % ZIPALIGN)
    print('-- apksigner: %s' % APKSIGNER)
    print('-- runtime jar: %s' % RUNTIME_JAR)
    print('-- keytool: %s' % KEYTOOL)
    print('-- abi: %s' % args.ndk_abi)
    print('-- min-platform-version: %d' % args.sdk_min_platform_version)
    print('-- platform-version: %d' % args.sdk_platform_version)

    if (args.command == 'configure'):
        if (not os.path.isfile(manifest_filepath)):
            if (not args.manifest):
                make_manifest(manifest_filepath, args)
            else:
                shutil.copyfile(args.manifest, manifest_filepath)
        setup_manifest(manifest_filepath, args)
        print('manifest: %s' % manifest_filepath)

    cmake_generator = 'Ninja' if args.ninja_path else '"Unix Makefiles"'
    cmake_make_bin = args.ninja_path.replace('\\', '/') if args.ninja_path else args.make_path.replace('\\', '/')
    cmake_toolchain_file = os.path.join(args.ndk_root, 'build', 'cmake', 'android.toolchain.cmake').replace('\\', '/')

    for abi in args.ndk_abi:
        build_dir = os.path.join(build_root, abi, args.build_type)
        if (not os.path.isdir(build_dir)):
            os.makedirs(build_dir)

        # configure
        if (args.command == 'configure'):
            print('')
            print('configure: %s' % abi)
            try:
                cmd = [
                    args.cmake_path, 
                    '-DCMAKE_TOOLCHAIN_FILE=' + cmake_toolchain_file,
                    '-DANDROID_ABI=' + abi,
                    '-DANDROID_PLATFORM=android-' + str(args.sdk_min_platform_version),
                    '-DCMAKE_MAKE_PROGRAM=' + cmake_make_bin,
                    '-DCMAKE_EXPORT_COMPILE_COMMANDS=1',
                    '-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=.',
                    '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=.',
                    '-DBUNDLE=1',
                    '-DBUNDLE_TARGET=' + args.cmake_target,
                    '-DBUNDLE_TARGET_NAME=' + args.name,
                    '-DCMAKE_BUILD_TYPE=' + args.build_type,
                    '-G', cmake_generator,
                    args.cmake_source]
                if (args.cmake_args):
                    cmd.extend(args.cmake_args.split(' '))
                logging.debug(' '.join(cmd))
                cmake_result = subprocess.check_output(cmd, cwd=build_dir)
            except subprocess.CalledProcessError as e:
                logging.error("cmake returned with error code: %d" % e.returncode)
                exit(-1)
            else:
                for l in cmake_result.decode('ascii').split('\n'):
                    logging.debug(l)

            # make vscode workspace files
            vscode_workspace = os.path.join(build_root, args.name + '-' + abi + '-' + args.build_type + '.code-workspace')
            make_vscode_workspace(vscode_workspace, src_root, args.cmake_source, args.cmake_path, build_dir, abi, args)

        # build
        if (args.command == 'build'):
            print('')
            print('build: %s' % abi)
            try:
                cmd = [
                    args.cmake_path,
                    '--build', '.',
                    '--target', args.cmake_target,
                    '--'
                ]
                logging.debug(' '.join(cmd))
                cmake_result = subprocess.check_output(cmd, cwd=build_dir)
            except subprocess.CalledProcessError as e:
                logging.error("cmake (build) returned with error code: %d" % e.returncode)
                for l in e.stdout.decode('ascii').split('\n'):
                    print(l)
                exit(-1)
            else:
                for l in cmake_result.decode('ascii').split('\n'):
                    logging.debug(l)

            # copy the file into 'lib' directory
            libs_dir = os.path.join(libs_root, abi)
            src_lib = os.path.join(build_dir, 'lib' + args.cmake_target + '.so')
            output_lib = os.path.join(libs_dir, 'lib' + args.name + '.so')
            if (not os.path.isdir(libs_dir)):
                os.makedirs(libs_dir)
            shutil.copyfile(src_lib, output_lib)
            print('output: %s' % output_lib)

    if (args.command == 'configure'):
        # copy dummy resource file(s)
        if (not os.path.isdir(res_root)):
            print('copying initial resource files to: %s/res' % args.target_dir.replace('\\', '/'))
            cur_dir = os.path.dirname(os.path.abspath(__file__))
            with zipfile.ZipFile(os.path.join(cur_dir, 'android-res.zip'), 'r') as f:
                f.extractall(args.target_dir)

        # generate resources java files
        try:
            cmd = [
                AAPT, 
                'package',
                '-v', '-f', '-m',
                '-S', 'res',
                '-J', 'src',
                '-M', 'AndroidManifest.xml',
                '-I', ANDROID_JAR
            ]
            logging.debug(' '.join(cmd))
            aapt_result = subprocess.check_output(cmd, cwd=args.target_dir)
        except subprocess.CalledProcessError as e:
            logging.error("aapt returned with error code: %d" % e.returncode)
            exit(-1)
        else:
            for l in aapt_result.decode('ascii').split('\n'):
                logging.debug(l)

    if (args.command == 'build'):
        java_compile_sources(args, java_class_root, src_root, apk_root, RUNTIME_JAR, ANDROID_JAR, DEX)

    if (args.command == 'package'):
        # make APK
        tmp_apk = 'build/' + args.name + (('-' + args.app_version_name) if args.app_version_name else '') + '-unaligned.apk'
        final_apk = 'build/' + args.name + (('-' + args.app_version_name) if args.app_version_name else '') + '.apk'

        try:
            cmd = [
                AAPT, 'package',
                '-v', '-f',
                '-S', 'res',
                '-M', 'AndroidManifest.xml',
                '-I', ANDROID_JAR,
                '-F', tmp_apk,
                'apk'
            ]
            logging.debug(' '.join(cmd))
            aapt_result = subprocess.check_output(cmd, cwd=args.target_dir)
        except subprocess.CalledProcessError as e:
            logging.error('aapt returned with error code: %d' % e.returncode)
            exit(-1)
        else:
            for l in aapt_result.decode('ascii').split('\n'):
                logging.debug(l)
            print('temp APK: %s' % tmp_apk)

        # zipalign
        try:
            cmd = [
                ZIPALIGN,
                '-f', '4',
                tmp_apk,
                final_apk
            ]
            logging.debug(' '.join(cmd))
            zipalign_result = subprocess.check_output(cmd, cwd=args.target_dir)
        except subprocess.CalledProcessError as e:
            logging.error('zipalign returned with error code: %d' % e.returncode)
            exit(-1)
        else:
            for l in zipalign_result.decode('ascii').split('\n'):
                logging.debug(l)
            print('final APK: %s' % final_apk)
        
        # create temp signing key if not provided
        if not args.keystore:
            debug_keystore = os.path.join(build_root, 'debug.keystore')
            if not os.path.isfile(debug_keystore):
                try:
                    cmd = [
                        KEYTOOL, '-genkeypair',
                        '-keystore', 'build/debug.keystore',
                        '-storepass', 'android',
                        '-alias', 'androiddebugkey',
                        '-keypass', 'android',
                        '-keyalg', 'RSA',
                        '-validity', '10000',
                        '-dname', 'CN=,OU=,O=,L=,S=,C='
                    ]
                    logging.debug(' '.join(cmd))
                    keytool_result = subprocess.check_output(cmd, cwd=args.target_dir)
                except subprocess.CalledProcessError as e:
                    logging.error('zipalign returned with error code: %d' % e.returncode)
                    exit(-1)
                else:
                    print('debug.keystore: %s' % debug_keystore)
                    for l in keytool_result.decode('ascii').split('\n'):
                        logging.debug(l)        
            args.keystore = 'build/debug.keystore'
            args.keystore_password = 'android'
            args.key_alias = 'androiddebugkey'
            args.key_password = 'android'
        
        # sign the APK
        try:
            cmd = [
                APKSIGNER, 'sign',
                '-v',
                '--ks', args.keystore,
                '--ks-pass', 'pass:' + args.keystore_password,
                '--key-pass', 'pass:' + args.key_password,
                '--ks-key-alias', args.key_alias,
                final_apk
            ]
            logging.debug(' '.join(cmd))
            sign_result = subprocess.check_output(cmd, cwd=args.target_dir)
        except subprocess.CalledProcessError as e:
            logging.error('apksigner returned with error code: %d' % e.returncode)
            exit(-1)
        else:
            for l in sign_result.decode('ascii').split('\n'):
                logging.debug(l)    
            print('signed: %s' % final_apk)

        # install the apk to the device
        if args.install:
            apk_hashfile = os.path.join(args.target_dir, final_apk + '.sha1')
            apk_sha1 = ''
            apk_new_sha1 = ''
            if os.path.isfile(apk_hashfile):
                with open(apk_hashfile, 'rt') as f:
                    apk_sha1 = f.readline()
                    f.close() 
            with open(os.path.join(args.target_dir, final_apk), 'rb') as f:
                sha1 = hashlib.sha1()
                while True:
                    data = f.read(8192)
                    if not data:
                        break
                    sha1.update(data)
                f.close()
                apk_new_sha1 = sha1.hexdigest()

            if (apk_sha1 == apk_new_sha1):
                print('APK not changed, skipping installation')
            else:
                # compare hash and decide if we need to install
                try:
                    cmd = [ADB, 'install', '-r', final_apk]
                    logging.debug(' '.join(cmd))
                    adb_result = subprocess.check_output(cmd, cwd=args.target_dir)
                except subprocess.CalledProcessError as e:
                    logging.error('adb returned with error code: %d' % e.returncode)
                    exit(-1)
                else:
                    for l in adb_result.decode('ascii').split('\n'):
                        logging.debug(l)    
                    if adb_result.decode('ascii').find('Failure') != -1:
                        # try uninstalling the app and reinstall
                        try:
                            print('install failed: trying uninstall ...')
                            cmd = [ADB, 'uninstall', args.package]
                            logging.debug(' '.join(cmd))
                            adb_result = subprocess.check_output(cmd, cwd=args.target_dir)

                            cmd = [ADB, 'install', '-r', final_apk]
                            logging.debug(' '.join(cmd))
                            adb_result = subprocess.check_output(cmd, cwd=args.target_dir)
                        except:
                            logging.error('adb returned with error code: %d' % e.returncode)
                            exit(-1)
                        else:
                            for l in adb_result.decode('ascii').split('\n'):
                                logging.debug(l)    
                            print('adb install: %s' % final_apk)                       
                    print('adb install: %s' % final_apk)
                
                # write hash
                with open(apk_hashfile, 'wt') as f:
                    f.write(apk_new_sha1)
                    f.close()

        # copy the apk to the destination path
        if args.copy:
            shutil.copyfile(os.path.join(args.target_dir, final_apk), args.copy)
            print('copied to: %s' % args.copy)

    if args.command == 'debug':
        # find proper gdbserver
        adb_upload_debugger(ADB, args.ndk_root, args.package, 'gdbserver')

    if args.command == 'deploy':
        # read AndroidManifest.xml and extract the running activity
        activity_name = get_activity_name(ADB, args.package, manifest_filepath)
        cmd = [ADB, 'shell', 'am', 'start', args.package + '/' + activity_name]
        logging.debug(' '.join(cmd))
        if subprocess.call(cmd) != 0:
            logging.error('running app {} failed'.format(cmd))
        process_id = adb_get_process_id(ADB, args.package)

        # run logcat
        cmd = [
            ADB, 'logcat',  
            '-v' 'color',
            '-v', 'raw',
            '-v', 'printable',
            '-v', 'process']
        logging.debug(' '.join(cmd))
        p_log = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        stdout_reader = AsyncFileReader(p_log.stdout, process_id)
        stdout_reader.start()
        stdout_reader.join()


if __name__ == "__main__":
    # ENV vars
    arg_parser = argparse.ArgumentParser(description='rizz android build tool v1.0')
    arg_parser.add_argument('command', help='build command', choices=['configure', 'build', 'package', 'debug', 'deploy'])
    arg_parser.add_argument('--name', help='app/cmake target name', required=True)
    arg_parser.add_argument('--cmake-source', help='cmake (native) source directory')
    arg_parser.add_argument('--cmake-target', help='cmake (native) project name (default is same as `name`)')
    arg_parser.add_argument('--cmake-args', help='additional cmake arguments', default='')
    arg_parser.add_argument('--build-type', help='build type', choices=['Debug', 'Release', 'RelWithDebInfo'], default='Debug')
    arg_parser.add_argument('--target-dir', help='target build directory', required=True)
    arg_parser.add_argument('--package', help='Java package name')
    arg_parser.add_argument('--app-version', help='app incremental version number', type=int, default=1)
    arg_parser.add_argument('--app-version-name', help='app version name (example: 1.1.0)', default='1.0.0')
    arg_parser.add_argument('--sdk-root', help='path to android sdk root directory', 
        default=(os.environ['ANDROID_SDK_ROOT'] if 'ANDROID_SDK_ROOT' in os.environ else ''))
    arg_parser.add_argument('--ndk-root', help='path to android ndk root directory', 
        default=(os.environ['ANDROID_NDK_ROOT'] if 'ANDROID_NDK_ROOT' in os.environ else ''))
    arg_parser.add_argument('--java-root', help='java runtime root directory', 
        default=(os.environ['JAVA_HOME'] if 'JAVA_HOME' in os.environ else ''))
    arg_parser.add_argument('--ndk-abi', help='NDK ABI type', action='append', choices=['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'])
    arg_parser.add_argument('--sdk-min-platform-version', help='Minimum Android platform version', type=int, default=23)
    arg_parser.add_argument('--sdk-platform-version', help='Android platform version', type=int, default=23)
    arg_parser.add_argument('--sdk-buildtools-version', help='build tools version', default='27.0.3')
    arg_parser.add_argument('--version', action='version', version='%(prog)s 1.0')
    arg_parser.add_argument('--keystore', help='key store file path', default='')
    arg_parser.add_argument('--key-alias', help='key alias in the key store file', default='')
    arg_parser.add_argument('--keystore-password', help='password for keystore file', default='')
    arg_parser.add_argument('--key-password', help='key password', default='')
    arg_parser.add_argument('--manifest', help='custom manifest xml file', default='')
    arg_parser.add_argument('--install', help='installs the final APK to the device (adb install)', action='store_true')
    arg_parser.add_argument('--copy', help='copy the final APK to the specified path')
    arg_parser.add_argument('--verbose', help='enable verbose output', action='store_const', const=True)
    args = arg_parser.parse_args(sys.argv[1:])

    if ((not args.cmake_source or not os.path.isdir(args.cmake_source)) and args.command == 'configure'):
        logging.error('--cmake-source directory is invalid: %s' % (args.cmake_source if args.cmake_source else ''))
        exit(-1)
    args.cmake_source = os.path.normpath(args.cmake_source)

    if (not args.package and not args.manifest and (args.command == 'configure' or args.command == 'debug' or args.command == 'deploy')):
        logging.error('--package argument is not provided')
        exit(-1)

    if (not args.sdk_root or not os.path.isdir(args.sdk_root)):
        logging.error('sdk_root directory is invalid: %s' % args.sdk_root)
        exit(-1)
    args.sdk_root = os.path.normpath(args.sdk_root)
        
    if (not args.ndk_root or not os.path.isdir(args.ndk_root)):
        args.ndk_root = os.path.join(args.sdk_root, 'ndk-bundle')
        if (not os.path.isdir(args.ndk_root)):
            logging.error('ndk_root directory is invalid: %s' % args.ndk_root)
            exit(-1)
    args.ndk_root = os.path.normpath(args.ndk_root)

    if (not args.java_root or not os.path.isdir(args.java_root)):
        logging.error('java_root directory is invalid: %s' % args.java_root)
        exit(-1)
    args.java_root = os.path.normpath(args.java_root)

    if (args.keystore and not os.path.isfile(args.keystore)) and args.command == 'package':
        logging.error('invalid keystore file: %s' % args.keystore)
        exit(-1)
    if (args.keystore):
        args.keystore = os.path.normpath(args.keystore)
    
    if (args.manifest and not os.path.isfile(args.manifest)) and args.command == 'configure':
        logging.error('invalid manifest file: %s' % args.manifest)
        exit(-1)
    if (args.manifest):
        args.manifest = os.path.normpath(args.manifest)

    if (not args.cmake_target):
        args.cmake_target = args.name

    args.target_dir = os.path.normpath(args.target_dir)
    
    if (not args.ndk_abi):
        args.ndk_abi = ['armeabi-v7a']

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

    build_android(args)

