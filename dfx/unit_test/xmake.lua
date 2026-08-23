target("unit_test")
    set_kind("binary")
    set_default(false)
    add_files("*.cpp")
    
    add_includedirs("$(projectdir)/dfx/internal", {public = true})

    -- Catch2 单文件在 debug（不开优化）下节数超限，MSVC 需要 /bigobj
    if is_plat("windows") then
        add_cxflags("/bigobj", {force = true})
    end
