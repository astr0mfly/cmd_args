target("once")
    set_kind("binary")
    set_symbols("debug")
    set_default(false)
    add_files("*.cpp")
    add_includedirs("$(projectdir)/include", {public = true})

    -- airbag 的 thread_error 用 _set_se_translator()，MSVC 需要 /EHa 才编译通过
    if is_plat("windows") then
        add_cxflags("/EHa", {force = true})
    end