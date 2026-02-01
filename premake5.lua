require "premake/export-compile-commands"

workspace "Billyprints"
    architecture "x64"
    startproject "Billyprints"

    configurations { "Debug", "Release" }

    outputstr = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Billyprints"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("bin/" .. outputstr .. "/%{prj.name}")
    objdir ("bin-int/" .. outputstr .. "/%{prj.name}")

    files {
        "billyprints/**.h",
        "billyprints/**.hpp",
        "billyprints/**.cpp",
        "libs/imgui/*.h",
        "libs/imgui/*.hpp",
        "libs/imgui/*.cpp",
        "libs/imnodes/*.h",
        "libs/imnodes/*.cpp",
        "libs/backends/*.h",
        "libs/backends/*.hpp",
        "libs/backends/*.cpp"
    }

    removefiles {
        "billyprints/Nodes/Gates/legacy/**"
    }

    includedirs {
        "billyprints",
        "billyprints/Nodes",
        "billyprints/Nodes/Gates",
        "billyprints/Nodes/Special",
        "billyprints/Editor",
        "libs/glfw/include",
        "libs/imgui",
        "libs/imnodes",
        "libs/backends",
        "libs/cpp-httplib",
        "libs/nlohmann",
        "C:/Users/BIT/scoop/apps/openssl/current/include"
    }

    libdirs {
        "libs/glfw/lib-vc2010-64",
        "C:/Users/BIT/scoop/apps/openssl/current/lib"
    }

    links {
        "glfw3",
        "opengl32",
        "gdi32",
        "shell32",
        "ws2_32",
        "crypt32",
        "libssl",
        "libcrypto"
    }

    -- pchheader "pch.hpp"
    -- pchsource "billyprints/pch.cpp"

    filter "system:windows"
        systemversion "latest"
        defines { "_CRT_SECURE_NO_WARNINGS", "CPPHTTPLIB_OPENSSL_SUPPORT" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        staticruntime "Off"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "On"
