-- reactive：响应式数据示例（Property 写/读拦截 + 变更通知）
target("reactive")
    set_kind("binary")
    set_symbols("debug")
    set_default(false)
    add_files("*.cpp")
    add_includedirs("$(projectdir)/include", {public = true})
