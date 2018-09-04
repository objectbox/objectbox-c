from shutil import copy2, rmtree
from tempfile import mkdtemp

from conans import ConanFile, tools
import os.path

class ObjectboxC(ConanFile):
    name = "objectbox-c"
    version = "0.1"
    settings = "os", "arch"
    description = "C Library for ObjectBox - a super fast embedded database for objects"
    url = "https://github.com/objectbox/objectbox-c"
    license = "Apache-2"

    # Defaults for Linux, Mac, etc.
    obxBuildDir = "../cbuild/Release/objectbox-c"
    obxTestExe = obxBuildDir + "/objectbox-c-test"

    def package(self):
        if self.settings.os == "Windows":
            self.obxBuildDir = "../visual-studio/x64/Release"
            dll_src = self.obxBuildDir + "/objectbox-c.dll"
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
            self.copy(self.obxBuildDir + "/libobjectbox.so", dst="lib")
            self.copy(self.obxBuildDir + "/libobjectbox.dylib", dst="lib")

        # Platform independent
        self.copy("include/*.h")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)

    def test(self):
        if self.settings.os != "Windows":
            self.run("pwd")  # directory is build/_hash_, thus go up 2 additional levels
            exe = "../../" + self.obxTestExe
            self.run("ls -l " + exe)
            self.run(exe)
