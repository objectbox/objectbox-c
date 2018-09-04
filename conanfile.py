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
    obxLibSo = obxBuildDir + "/libobjectbox.so"
    obxLibDy = obxBuildDir + "/libobjectbox.so"
    obxLibDll = ""

    def package(self):
        if self.settings.os == "Windows":
            self.obxBuildDir = "../visual-studio/x64/Release"
            self.obxLibDll = self.obxBuildDir + "/objectbox.dll"
            if not os.path.isfile(self.obxLibDll):
                raise Exception("DLL does not exist: " + self.obxLibDll)
        else:
            self.run("./build.sh release", cwd="..")

        self.copy("include/*.h")
        self.copy(self.obxLibSo, dst="lib")
        self.copy(self.obxLibDy, dst="lib")
        self.copy(self.obxLibDll, dst="lib")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)

    def test(self):
        if self.settings.os != "Windows":
            self.run("pwd")  # directory is build/_hash_, thus go up 2 additional levels
            exe = "../../" + self.obxTestExe
            self.run("ls -l " + exe)
            self.run(exe)
