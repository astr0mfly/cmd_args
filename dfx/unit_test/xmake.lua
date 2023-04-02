target("unit_test")
    set_kind("binary")
    set_default(false)
    add_files("*.cpp")
    
    add_includedirs("$(projectdir)/dfx/internal", {public = true})
