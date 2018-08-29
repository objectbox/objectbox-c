from conans import ConanFile, tools


class ObjectboxC(ConanFile):
    name = "objectbox-c"
    version = "0.1"
    settings = "os", "arch"
    description = "C Library for ObjectBox - a super fast embedded database for objects"
    url = "https://github.com/objectbox/objectbox-c"
    license = "Apache-2"

    obxBuildDir = "../cbuild/Release/objectbox-c"
    obxTestExe = obxBuildDir + "/objectbox-c-test"
    obxLibSo = obxBuildDir + "/libobjectboxc.so"
    obxLibDy = obxBuildDir + "/libobjectboxc.so"

    def package(self):
        self.run("./build.sh release", cwd="..")
        self.copy("include/*.h")
        self.copy(self.obxLibSo, dst="lib")
        self.copy(self.obxLibDy, dst="lib")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)

    def test(self):
        self.run("pwd")  # directory is build/_hash_, thus go up 2 additional levels
        exe = "../../" + self.obxTestExe
        self.run("ls -l " + exe)
        self.run(exe)
