workspace "main"
    architecture "x64"
    toolset "gcc"

    cppdialect ("C++23")

    configurations {
        "debug",
        "release",
        "dist"
    }

    filter "configurations:debug"
        defines {"_DEBUG", "_PROFILING"}
        runtime "Debug"
        symbols "On"
        filter {"system:linux", "action:gmake"}
            buildoptions {"-g"} -- For core files
    filter "configurations:release"
        defines {"_RELEASE", "NDEBUG", "_PROFILING"}
        runtime "Release"
        optimize "Speed"
        symbols "On"
    filter "configurations:dist"
        defines {"_RELEASE", "NDEBUG", "_PROFILING"}
        runtime "Release"
        optimize "Speed"
        symbols "Off"

    filter "system:macosx"
        defines {"_MACOSX"}

    filter "system:linux"
        defines {"_LINUX"}

    filter "system:windows"
        defines {"_WINDOWS", "uint=uint32_t"}

outputdir = "%{cfg.buildcfg}/%{cfg.system}/%{cfg.architectrure}"
project "VMesh-cli"
    kind "ConsoleApp"
    language "C++"
    targetname "vmesh"
    targetdir ("bin/" .. outputdir)
    objdir ("bin-int/" .. outputdir)

    files {
        "src/**.hpp",
        "src/**.cpp",
        "include/**.hpp",
        "include/**.cpp",
        "dependencies/src/**.cpp",
        "dependencies/src/**.c",
        "dependencies/include/**.h",
        "dependencies/include/**.hpp"
    }

    includedirs {
        "include",
        "dependencies/include",
        "/usr/include"
    }

    libdirs {
        "dependencies/libs"
    }

    links {
      "pthread",
      "boost_program_options",
      "assimp",
      "VMesh"
    }

