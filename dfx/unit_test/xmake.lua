target("unit_test")
    set_kind("binary")
    set_default(false)
    add_files("*.cpp")
    
    add_includedirs("$(projectdir)/dfx/internal", {public = true})

    -- 多 TU 并行编译时共用同一 PDB，MSVC 需要 /FS 允许并发写入
    if is_plat("windows") then
        add_cxflags("/FS", {force = true})
    end
