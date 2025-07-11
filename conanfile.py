from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os

class FastPdfParserConan(ConanFile):
    name = "fast-pdf-parser"
    version = "0.1.0"
    license = "MIT"
    author = "Fast PDF Parser Team"
    url = "https://github.com/mboros1/fast-pdf-parser"
    description = "High-performance C++ PDF text extraction library"
    topics = ("pdf", "text-extraction", "mupdf", "parsing")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True
    }
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*", "benchmark/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        # JSON library
        self.requires("nlohmann_json/3.11.3")
        # Testing framework
        self.test_requires("gtest/1.14.0")
        # Benchmarking
        self.test_requires("benchmark/1.8.3")
        # Note: MuPDF must be installed separately as it's not available in Conan Center

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["fastpdfparser"]
        self.cpp_info.set_property("cmake_file_name", "fast-pdf-parser")
        self.cpp_info.set_property("cmake_target_name", "fast-pdf-parser::fastpdfparser")