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
            return "../cbuild/" + self.getArch() + "/objectbox-c/Release"
        else:
            return "../cbuild/" + os.environ.get('OBX_CMAKE_TOOLCHAIN', '') + "/Release/objectbox-c"

    def getArch(self):
        if self.settings.arch == "x86_64":
            return "x64"
        else:
            return str(self.settings.arch)

    def copyOrFail(self, lib):
        if not os.path.isfile(lib):
            raise Exception("Library does not exist: " + lib)
        self.copy(lib, dst="lib")

    def package(self):
        obxBuildDir = self.buildDir()
        if self.settings.os == "Windows":
            self.run(".\\ci\\build.bat " + self.getArch() +" Release objectbox", cwd="..")
            self.copyOrFail(obxBuildDir + "/objectbox.dll")
        else:
            self.run("./build.sh release", cwd="..")

            if self.settings.os == "Linux":
                self.copyOrFail(obxBuildDir + "/libobjectbox.so")
            else:
                self.copyOrFail(obxBuildDir + "/libobjectbox.dylib")

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
            c_cpp_test_exe = build_dir + "/objectbox-c-cpp-test"
            self.run("ls -l " + c_cpp_test_exe)
            self.run(c_cpp_test_exe, cwd=c_cpp_test_temp_dir)
            rmtree(c_cpp_test_temp_dir)
