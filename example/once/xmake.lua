target("once")
    set_kind("binary")
    set_symbols("debug")
    set_default(false)
    add_files("*.cpp")
    
    add_includedirs("$(projectdir)/include", {public = true})
