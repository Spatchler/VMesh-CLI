workspace "main"
    architecture "x64"
    toolset "gcc"

    buildoptions "--std=c++23"

    configurations {
        "debug",
        "release",
        "dist"
    }

    filter "configurations:debug"
        defines {"_DEBUG", "_PROFILING"}
        symbols "On"
        filter {"system:linux", "action:gmake"}
            buildoptions {"-g"} -- For core files
    filter "configurations:release"
        defines {"_RELEASE", "NDEBUG", "_PROFILING"}
        optimize "On"
    filter "configurations:dist"
        defines {"_RELEASE", "NDEBUG", "_PROFILING"}
        optimize "Speed"

    filter "system:macosx"
        defines {"_MACOSX"}

    filter "system:linux"
        defines {"_LINUX"}

    filter "system:windows"
        defines {"_WINDOWS"}

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
        "boost_program_options",
        "assimp",
        "VMesh"
    }

