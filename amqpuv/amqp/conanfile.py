from conans import ConanFile, tools


class AmqpConan(ConanFile):
    name = "amqp"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    description = "AMQP C++ messaging wrapper library"
    url = "None"
    license = "None"
    author = "None"
    topics = None

    def package(self):
        self.copy("lib/*")
        self.copy("messaging.h", "include/")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
