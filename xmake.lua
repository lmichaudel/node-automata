set_project("node-automata")
add_rules("plugin.compile_commands.autoupdate")
set_toolchains("clang")
set_version("0.1.0")

set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.release")

add_requires("libsdl3", "libsdl3_image", "glm")

includes("shaders/xmake.lua")

target("node-automata")
    set_kind("binary")
    add_files("src/**.cpp")
    add_files("shaders/**.hlsl", {rule = "hlsl"})
    add_headerfiles("src/**.hpp")
    add_includedirs("src")
    add_packages("libsdl3", "libsdl3_image", "glm")

    after_build(function (target)
        local asset_dir = path.join(os.projectdir(), "assets")
        if os.isdir(asset_dir) then
            local target_asset_dir = path.join(target:targetdir(), "assets")
            os.rm(target_asset_dir)
            os.cp(asset_dir, target_asset_dir)
        end
    end)

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
