target("declare")
    set_kind("binary")
    set_symbols("debug")
    set_default(false)
    add_files("*.cpp")
    add_includedirs("$(projectdir)/include", {public = true})

    if is_plat("windows") then
        add_cxflags("/EHa", {force = true})
    end
