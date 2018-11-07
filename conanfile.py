from shutil import copy2, rmtree
from tempfile import mkdtemp
from conans import ConanFile, tools
import os.path

class ObjectboxC(ConanFile):
    name = "objectbox-c"
    settings = "os", "arch"
    description = "C Library for ObjectBox - a super fast embedded database for objects"
    url = "https://github.com/objectbox/objectbox-c"
    license = "Apache-2"

    def buildDir(self):
        if self.settings.os == "Windows":
            return "../visual-studio/x64/Release"
        else:
            return "../cbuild/" + os.environ.get('OBX_CMAKE_TOOLCHAIN', '') + "/Release/objectbox-c"

    def package(self):
        obxBuildDir = self.buildDir()
        if self.settings.os == "Windows":
            dll_src = obxBuildDir + "/objectbox-c.dll"
            if not os.path.isfile(dll_src):
                raise Exception("DLL does not exist: " + dll_src)

            # lib name differs in VS build, thus copy it to temp dir with the correct name for packaging
            temp_dir = mkdtemp()
            dll_dst = temp_dir + "/objectbox.dll"
            copy2(dll_src, dll_dst)

            self.copy(dll_dst, dst="lib")
            rmtree(temp_dir)
        else:
            self.run("./build.sh release", cwd="..")
            self.copy(obxBuildDir + "/libobjectbox.so", dst="lib")
            self.copy(obxBuildDir + "/libobjectbox.dylib", dst="lib")

        # Platform independent
        self.copy("include/*.h")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)

    def test(self):
        # NOTE .os_build is not defined
        #  if self.settings.os != self.settings.os_build or self.settings.arch != self.settings.arch_build:
        if os.environ.get('OBX_CMAKE_TOOLCHAIN', '') != "":
            print("No tests executed because this is a cross-compilation")

        elif self.settings.os != "Windows":
            print("********* Test dirs **********")
            self.run("echo \"PWD: $(pwd)\"")  # directory is build/_hash_, thus go up 2 additional levels
            build_dir = os.path.abspath("../../" + self.buildDir())
            print("Build dir: " + build_dir)

            print("********* C tests **********")
            c_test_temp_dir = mkdtemp(dir=build_dir)
            print("C test dir: " + c_test_temp_dir)
            c_test_exe = build_dir + "/src-test/objectbox-c-test"
            self.run("ls -l " + c_test_exe)
            self.run(c_test_exe, cwd=c_test_temp_dir)
            rmtree(c_test_temp_dir)

            print("********* C/CPP tests **********")
            c_cpp_test_temp_dir = mkdtemp(dir=build_dir)
            print("C CPP test dir: " + c_cpp_test_temp_dir)
            c_cpp_test_exe = build_dir + "/test/objectbox-c-cpp-test"
            self.run("ls -l " + c_cpp_test_exe)
            self.run(c_cpp_test_exe, cwd=c_cpp_test_temp_dir)
            rmtree(c_cpp_test_temp_dir)
