rule("resources")
    before_build(function (target)
        import("core.project.depend")
        import("lib.detect.find_tool")

        local generator = find_tool("msdf-atlas-gen")
        if not generator then
            raise("msdf-atlas-gen is required to generate font atlases")
        end

        local font_dir = path.join(os.projectdir(), "res", "fonts")
        for _, font_file in ipairs(os.files(path.join(font_dir, "**.ttf"))) do
            local font_name = path.basename(font_file)
            local image_output = path.join(font_dir, font_name .. ".png")
            local json_output = path.join(font_dir, font_name .. ".json")
            local charset_file = path.join(font_dir, font_name .. ".charset")
            local dependencies = {font_file}
            if os.isfile(charset_file) then
                table.insert(dependencies, charset_file)
            end

            depend.on_changed(function ()
                local arguments = {
                    "-font", font_file,
                    "-type", "msdf",
                    "-size", "96",
                    "-pxrange", "8",
                    "-yorigin", "top",
                    "-imageout", image_output,
                    "-json", json_output
                }
                if os.isfile(charset_file) then
                    table.insert(arguments, 3, charset_file)
                    table.insert(arguments, 3, "-charset")
                end
                os.vrunv(generator.program, arguments)
            end, {
                dependfile = target:dependfile(image_output),
                files = dependencies,
                changed = target:is_rebuilt() or not os.isfile(image_output) or
                          not os.isfile(json_output)
            })
        end
    end)

    after_build(function (target)
        local source_dir = path.join(os.projectdir(), "res")
        local destination_dir = path.join(target:targetdir(), "res")
        os.rm(path.join(target:targetdir(), "assets"))
        os.rm(destination_dir)
        os.cp(source_dir, destination_dir)
        os.rm(path.join(destination_dir, "xmake.lua"))
    end)
