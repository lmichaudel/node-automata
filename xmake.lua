set_project("node-automata")
add_rules("plugin.compile_commands.autoupdate")
set_toolchains("clang")
set_version("0.1.0")

set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.release")

add_requires("libsdl3", "libsdl3_image", "glm", "nlohmann_json")
add_requires("imgui", {configs = {sdl3 = true, sdl3_gpu = true}})

includes("shaders/xmake.lua", "res/xmake.lua")

target("node-automata")
    set_kind("binary")
    add_rules("resources")
    add_files("src/**.cpp")
    add_files("shaders/**.hlsl", {rule = "hlsl"})
    add_headerfiles("src/**.hpp")
    add_includedirs("src")
    add_packages("libsdl3", "libsdl3_image", "glm", "nlohmann_json", "imgui")

    if is_plat("windows") then
        add_cxxflags("/GR-", "/EHs-c-", {force = true})
    else
        add_cxxflags("-fno-rtti", "-fno-exceptions", {force = true})
    end

    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    else
        set_symbols("hidden")
        set_optimize("fastest")
        add_defines("NDEBUG")
    end
