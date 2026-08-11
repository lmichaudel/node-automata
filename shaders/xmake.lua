rule("hlsl")
    set_extensions(".hlsl")
    on_build_file(function (target, sourcefile, opt)
        import("core.project.depend")

        local stage
        if sourcefile:find("%.vert%.hlsl$") then
            stage = "vertex"
        elseif sourcefile:find("%.frag%.hlsl$") then
            stage = "fragment"
        else
            raise("shader filename must end in .vert.hlsl or .frag.hlsl: %s", sourcefile)
        end

        local destination
        local extension
        if is_plat("macosx") then
            destination = "MSL"
            extension = "msl"
        elseif is_plat("windows") then
            destination = "DXIL"
            extension = "dxil"
        else
            destination = "SPIRV"
            extension = "spv"
        end

        local shader_name = path.basename(sourcefile):gsub("%.hlsl$", "")
        local output = path.join(target:targetdir(), "shaders", shader_name .. "." .. extension)
        depend.on_changed(function ()
            os.mkdir(path.directory(output))
            os.vrunv("shadercross", {
                sourcefile,
                "-s", "HLSL",
                "-d", destination,
                "-t", stage,
                "-e", "main",
                "-o", output
            })
        end, {dependfile = target:dependfile(output), files = {sourcefile}, changed = target:is_rebuilt()})
    end)
